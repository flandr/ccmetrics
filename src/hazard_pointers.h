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

#ifndef SRC_METRICS_HAZARD_POINTERS_H_
#define SRC_METRICS_HAZARD_POINTERS_H_

#include <array>
#include <atomic>
#include <cassert>
#include <unordered_set>
#include <vector>

namespace ccmetrics {

template<typename T, int K> class HazardPointer;

/**
 * Safe memory reclamation for all your write-seldom read-often
 * lock-free data structure needs.
 *
 * Hazard Pointers [1] allow safe memory reclamation in non- garbage-collecting
 * systems where it is otherwise difficult to determine when to release
 * memory that may be shared by multiple threads. They are an alternative
 * to reference counting or reader-writer locks, allowing creation of lock-free
 * (though not wait-free, in the basic implementation) data structures without
 * DCAS, ABA-sensitive tagged pointers, or incurring expensive CAS operations
 * on the read path.
 *
 * Threads allocate a `HazardPointer` object and assign one or more references
 * to it via `setHazard` to indicate hazardous access to the value (i.e.,
 * indicating an intent to read the value at some time in the future), and
 * release the value with `clearHazard`. When a value is scheduled to be
 * deleted, the mutating thread must atomically make the reference unobtainable
 * and then release it with `retireNode`. The internal details ensure that
 * the memory will _eventually_ be freed, once no further readers can access
 * it. Threads are responsible for releasing hazard pointers when they are
 * no longer necessary, via `retire`. Note that the `allocate` and `retire`
 * operations synchronize on a shared hazard pointers list, so these operations
 * should be infrequent (ideally once each per thread lifetime).
 *
 * Determining when an object is safe to free has amortized constant cost.The
 * storage duration of HazardPointer objects is defined by the lifetime of their
 * parent HazardPointers object; typically this will be allocated as a static
 * member of a collection data type and will have static storage duration. The
 * storage overhead in this case is proportional to the maximum number of
 * threads (with small constant factor).
 *
 * [1] Michael, Maged M. "Hazard Pointers: Safe Memory Reclamation for
 * Lock-Free Objects," IEEE Transactions on Parallel and Distributed Systems.
 * 2004.
 */
template<typename T, int K = 1>
class HazardPointers {
public:
    typedef HazardPointer<T, K> pointer_type;

    HazardPointers() : head_(nullptr), hp_count_(0) { }
    ~HazardPointers();

    /** Allocate (or reuse) a hazard pointer. */
    HazardPointer<T, K>* allocate();

    /**
     * Retire the hazard pointer, _not its references_.
     *
     * It is the caller's responsibility to ensure that references are
     * properly retired before the record is retired, to ensure that
     * `helpScan` is able to reclaim the underlying memory later.
     */
    void retire(HazardPointer<T, K>* ptr);
private:
    std::atomic<HazardPointer<T, K>*> head_;
    std::atomic<int32_t> hp_count_;

    friend class HazardPointer<T, K>;
};

template<typename T, int K>
HazardPointers<T, K>::~HazardPointers() {
    auto *hp = head_.load();

    if (hp) {
        // Force cleanup to avoid memory leaks for small numbers of retired
        // nodes in hazard pointers
        hp->helpScan();
        hp->scan();
    }

    while (hp) {
        auto rm = hp;
        hp = hp->next_;
        delete rm;
    }
}

template<typename T, int K>
HazardPointer<T, K>* HazardPointers<T, K>::allocate() {
    auto* hp = head_.load(std::memory_order_acquire);
    while (hp) {
        bool inactive = false;
        if (hp->active_ || !hp->active_.compare_exchange_strong(
                inactive, true)) {
            hp = hp->next_;
            continue;
        }
        return hp;
    }

    // Note that we're adding a new hazard pointer, for scan() checks.
    hp_count_.fetch_add(1, std::memory_order_relaxed);

    HazardPointer<T, K> *oldhead = nullptr;
    hp = new HazardPointer<T, K>(this);
    // Relaxed is fine here; ordering for threads that can access this
    // value is enforced by the CAS below on the head pointer that publishes it
    hp->active_.store(true, std::memory_order_relaxed);
    do {
        oldhead = head_.load(std::memory_order_acquire);
        hp->next_ = oldhead;
    } while (!head_.compare_exchange_strong(oldhead, hp,
        std::memory_order_acq_rel));
    return hp;
}

template<typename T, int K>
void HazardPointers<T, K>::retire(HazardPointer<T, K> *hp) {
    for (int i = 0; i < K; ++i) {
        hp->pointers[i].store(nullptr, std::memory_order_release);
    }
    hp->active_.store(false, std::memory_order_release);
}

template<typename T, int K = 1>
class HazardPointer {
private:
    explicit HazardPointer(HazardPointers<T, K> *owner)
        : owner_(owner), pointers{}, next_(nullptr), active_(false) { }
    HazardPointer() = delete;
public:
    /**
     * Retire a value, freeing it if no hazardous references exist.
     *
     * This is the core of the Hazard Pointers SMR technique: when a thread
     * releases memory, it appends the value to the `retire_list` for
     * subsequent scanning (scanning is amortized over multiple retires).
     * During scan, the thread examines the active set of hazardous references
     * and checks whether the node is implicated in any of these. If it is
     * not, it frees the underlying memory; otherwise it is retained in
     * the `retire_list` and processed in a subsequent scan.
     *
     * A thread may not be able to free all the elements on its retire list
     * if it is forced to exit while hazardous references exist. The
     * `helpScan` method allows collaborative threads to clean up memory
     * eventually in this case.
     */
    void retireNode(T *node);

    /** Set the hazardous reference `k`. */
    void setHazard(int k, T* value) {
        assert(k < pointers.size());
        pointers[k].store(value, std::memory_order_release);
    }

    /** Set the hazardous reference. */
    void setHazard(T* value) {
        static_assert(K == 1, "This method removed for your protection");
        setHazard(0, value);
    }

    /**
     * Load an atomic value and set the hazard, looping as necessary to
     * prevent intervening mutations of the pointer.
     */
    T* loadAndSetHazard(std::atomic<T*> &value, int k) {
        T* cur = nullptr;
        do {
            cur = value.load(std::memory_order_acquire);
            setHazard(k, cur);
        } while (cur != value.load(std::memory_order_acquire));
        return cur;
    }

    /**
     * Try once to load and set the hazard, returning whether successful.
     */
    bool loadAndSetHazardOrFail(std::atomic<T*> &value, int k, T** ptr) {
        T* cur = value.load(std::memory_order_acquire);
        setHazard(k, cur);
        if (cur != value.load(std::memory_order_acquire)) {
            clearHazard(k);
            return false;
        }
        *ptr = cur;
        return true;
    }

    /** Clear the hazardous reference `k`. */
    void clearHazard(int k) {
        assert(k < pointers.size());
        pointers[k].store(nullptr, std::memory_order_release);
    }

    /** Clear the hazardous reference. */
    void clearHazard() {
        static_assert(K == 1, "This method removed for your protection");
        clearHazard(0);
    }
protected:
    // Visible for testing
    void scan();
private:
    bool shouldScan() {
        // Heuristic: clean when retire_list >= (R = H * (1 + 1/4)), ensuring
        // that checking for deletability of a node is constant (work
        // proportional to number of hazard pointers in the system amortized
        // over operations proportional to number of hazard pointers in the
        // system). Note that 'H' = list size * K.
        return retire_list_.size() >=
            owner_->hp_count_.load(std::memory_order_acquire) * K * (1 + 0.25);
    }

    void helpScan();

    HazardPointers<T, K> *owner_; // Owning collection

    std::array<std::atomic<T*>, K> pointers;
    HazardPointer *next_;
    std::atomic<bool> active_;
    std::vector<T*> retire_list_;

    friend class HazardPointers<T, K>;
};

template<typename T, int K>
void HazardPointer<T, K>::retireNode(T *node) {
    retire_list_.push_back(node);
    if (shouldScan()) {
        scan();
        helpScan();
    }
}

template<typename T, int K>
void HazardPointer<T, K>::scan() {
    std::unordered_set<T*> live;

    // Phase 1: accumulate all live hazard pointers
    auto *hp = owner_->head_.load(std::memory_order_acquire);
    while (hp) {
        for (int i = 0; i < K; ++i) {
            T* node = hp->pointers[i].load(std::memory_order_acquire);
            if (node != nullptr) {
                live.insert(node);
            }
        }
        hp = hp->next_;
    }

    // Phase 2: delete anything on the retire list that's not live
    auto iter = retire_list_.begin();
    while (iter != retire_list_.end()) {
        if (live.find(*iter) != live.end()) {
            // Still kicking
            ++iter;
            continue;
        }
        // Swap-and-delete vector cleanup idiom
        std::swap(*iter, retire_list_.back());
        delete retire_list_.back();
        retire_list_.pop_back();
    }
}

template<typename T, int K>
void HazardPointer<T, K>::helpScan() {
    // Process all HP nodes, cleaning up for anybody left nodes behind
    auto *hp = owner_->head_.load(std::memory_order_acquire);
    for ( ; hp != nullptr; hp = hp->next_) {
        if (hp == this) {
            continue;
        }
        bool inactive = false;
        if (!hp->active_.compare_exchange_strong(inactive, true)) {
            continue;
        }
        // We've locked the node; clear it by moving its remaining items
        // over to our retire list. We do this incrementally instead of
        // simply by assigning to ensure that we never try to free more than
        // the `R` retire limit at once. <-- This is a bit rigid.
        while (!hp->retire_list_.empty()) {
            retire_list_.push_back(hp->retire_list_.back());
            hp->retire_list_.pop_back();
            if (shouldScan()) {
                scan();
            }
        }
        hp->active_.store(false, std::memory_order_release);
    }
}

} // ccmetrics namespace

#endif // SRC_METRICS_HAZARD_POINTERS_H_
