/*
 * Copyright (©) 2015 Nate Rosenblum
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SRC_METRICS_STRIPED_INT64_H_
#define SRC_METRICS_STRIPED_INT64_H_

#include <atomic>
#include <cinttypes>

#include "cache_aligned.h"
#include "hazard_pointers.h"
#include "thread_local.h"

namespace ccmetrics {

class Striped64_Storage;

/**
 * A 64-bit signed value that may stripe values across 2+ storage locations
 * to reduce update contention.
 *
 * Memory access order is not enforced when 2 or more storage locations are
 * used; reads concurrent with multiple writes may observe only some of the
 * updated values. This class is not suitable for synchronization; it is
 * intended for uses that can tolerate such inconsistency, such as accumulating
 * values for counter metrics.
 *
 * See Doug Lea's [LongAdder](https://docs.oracle.com/javase/8/docs/api/java/
 * util/concurrent/atomic/LongAdder.html), which has been released into the
 * public domain.
 */
class Striped64 {
public:

    Striped64() : base_(0), stripes_(nullptr) { }
    // Basically just for testing
    explicit Striped64(int k);
    ~Striped64();

    /** @return the current value, with consistency caveats as above. */
    int64_t value();

    /** += value. */
    void add(int64_t value);
    void addSlow(int64_t value, Striped64_Storage* cur, size_t &hash_code);
private:
    std::atomic<int64_t> base_;
    std::atomic<Striped64_Storage*> stripes_;

    static HazardPointers<Striped64_Storage> stripes_pointers_;
    struct NewHPFunctor {
        decltype(stripes_pointers_)::pointer_type* operator()(void) const {
            return stripes_pointers_.allocate();
        }
    };
    static ThreadLocal<decltype(stripes_pointers_)::pointer_type, NewHPFunctor>
        stripes_hazard_;

    // TODO: perhaps CRTP-extension of ThreadLocal is a better way?
    struct NewHashCode {
        size_t* operator()(void) const;
    };
    static ThreadLocal<size_t, NewHashCode> thread_hash_code_;

    // Helper for use in releasing thread local stripes_hazard_
    static void retireHazard(void *h) {
        stripes_pointers_.retire(
            reinterpret_cast<decltype(stripes_pointers_)::pointer_type*>(h));
    }
};

// An enormously specialized non-contiguous array-like data structure.
// See the Striped64::add expansion path for insight.
class Striped64_Storage {
public:
    Striped64_Storage();
    ~Striped64_Storage();

    /**
     * Return an array of the next size (2*size), copying and clamining
     * ownership of elements in the other array. The existing arrary does
     * not relinquish ownership until disavow_all is invoked.
     */
    static Striped64_Storage* expand(Striped64_Storage *existing);

    /** @return the size of the array. */
    size_t size() const { return size_; }

    /** @return an array element. */
    std::atomic<int64_t>& get(size_t idx);

    /** Relinquish ownership of non-created elements in array. */
    void disavow();

    /** Relinquish ownership of all elements in array. */
    void disavow_all();
private:
    explicit Striped64_Storage(Striped64_Storage *other);

    bool clean_created_;
    bool owner_;
    size_t size_;
    CacheAligned<std::atomic<int64_t>> **data_;
};

inline void Striped64::add(int64_t value) {
    Striped64_Storage *cur = stripes_.load(std::memory_order_acquire);
    if (!cur) {
        // Attempt to update the base, checking for contention
        int64_t expected = base_.load(std::memory_order_relaxed);
        int64_t update = expected + value;
        if (base_.compare_exchange_strong(expected, update)) {
            // No contention; move along
            return;
        }
    }
    size_t& hash_code = *thread_hash_code_;
    if (cur) {
        // Is it really worth it to skip the hazard pointer on the
        // uncontended case?
        cur = stripes_hazard_->loadAndSetHazard(stripes_, 0);
        auto& slot = cur->get(hash_code & (cur->size() - 1));
        int64_t expected = slot;
        int64_t update = expected + value;
        if (slot.compare_exchange_strong(expected, update)) {
            stripes_hazard_->clearHazard(0);
            return;
        }
    }
    // Slow path
    addSlow(value, cur, hash_code);
}

} // ccmetrics namespace

#endif // SRC_METRICS_STRIPED_INT64_H_
