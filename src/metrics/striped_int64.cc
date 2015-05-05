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

#include "metrics/striped_int64.h"

#include <string.h>

#include "thread_local_random.h"

namespace ccmetrics {

HazardPointers<Striped64_Storage> Striped64::stripes_pointers_;

ThreadLocal<decltype(Striped64::stripes_pointers_)::pointer_type,
    Striped64::NewHPFunctor> Striped64::stripes_hazard_(Striped64::NewHPFunctor(),
        Striped64::retireHazard);

ThreadLocal<size_t, Striped64::NewHashCode> Striped64::thread_hash_code_{
    Striped64::NewHashCode()};

size_t* Striped64::NewHashCode::operator()(void) const {
    size_t* ret = new size_t(ThreadLocalRandom::current().next());
    return ret;
}

Striped64::~Striped64() {
    delete stripes_.load();
}

Striped64::Striped64(int k) : base_(0) {
    // Silly. But for testing only.
    Striped64_Storage *storage = new Striped64_Storage();
    while (storage->size() < k) {
        Striped64_Storage *tmp = Striped64_Storage::expand(storage);
        storage->disavow_all();
        delete storage;
        storage = tmp;
    }
    stripes_.store(storage);
}

int64_t Striped64::value() {
    Striped64_Storage *cur = nullptr;
    int64_t ret = base_;
    do {
        cur = stripes_.load(std::memory_order_acquire);

        // Short-circuit if there are no stripes; this never transitions
        // from non-null to null
        if (!cur) {
            return ret;
        }

        // Indicate our intent to dereference the stripes
        stripes_hazard_->setHazard(cur);
    } while(stripes_.load(std::memory_order_acquire) != cur);

    size_t cur_len = cur->size();
    for (int i = 0; i < cur_len; ++i) {
        ret += cur->get(i);
    }

    // Release the hazardous reference
    stripes_hazard_->clearHazard(0);

    return ret;
}

static const int STRIPE_LIMIT = 8; // XXX made up

void Striped64::add(int64_t value) {
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

    bool contended = false;
    size_t hash_code = *thread_hash_code_;
    for (;;) {
        if (!cur) {
            cur = new Striped64_Storage();
            Striped64_Storage *none = nullptr;
            if (stripes_.compare_exchange_strong(none, cur)) {
                // XXX you could just set the hazard and skip the load below
                continue;
            } else {
                delete cur; // NB leave it non-null for next iteration
                continue;
            }
        }

        do {
            cur = stripes_.load(std::memory_order_acquire);
            stripes_hazard_->setHazard(cur);
        } while(stripes_.load(std::memory_order_acquire) != cur);

        // Size is always a power of two
        size_t idx = hash_code & (cur->size() - 1);

        // 1. Try cas-update. If you succeed, you're done. Otherwise, indicate
        // contention & rehash & retry once w/o expanding.
        int64_t expected = cur->get(idx);
        int64_t update = expected + value;
        if (cur->get(idx).compare_exchange_strong(expected, update)) {
            break;
        }

        if (!contended) {
            contended = true;
        } else if (cur->size() < STRIPE_LIMIT) {
            // You can still grow. Grow.
            Striped64_Storage *next = Striped64_Storage::expand(cur);
            if (stripes_.compare_exchange_strong(cur, next)) {
                // Successfully grew the table; remember to free the existing
                // and then go do the update again
                cur->disavow_all();
                stripes_hazard_->retireNode(cur);
                // XXX you could have set the hazard and skipped the load on
                // the next go-round
            } else {
                // Raced with somebody else growing the table; release your
                // allocated storage and retry
                next->disavow();
                delete next;
            }
            continue;
        }

        // Remix the hash code
        hash_code ^= hash_code << 13;
        hash_code ^= hash_code >> 17;
        hash_code ^= hash_code << 5;
    }

    // [possibly] update thread hash code
    *thread_hash_code_ = hash_code;

    // Clear the hazard
    stripes_hazard_->clearHazard(0);
}

Striped64_Storage::Striped64_Storage() : clean_created_(true), owner_(true),
        size_(2) {
    auto *slab = new CacheAligned<std::atomic<int64_t>>[2];
    data_ = new CacheAligned<std::atomic<int64_t>>*[2] { slab, slab + 1};
}

Striped64_Storage::Striped64_Storage(Striped64_Storage *other)
        : clean_created_(true), owner_(true),
          size_(other->size_ << 1) {
    size_t slab_size = (other->size_ << 1) - other->size_;
    auto *slab = new CacheAligned<std::atomic<int64_t>>[slab_size];
    data_ = new CacheAligned<std::atomic<int64_t>>*[size_];
    memcpy(data_, other->data_, other->size_ * sizeof(*other->data_));
    for (size_t i = other->size_, j = 0; i < size_; ++i, ++j) {
        data_[i] = slab + j;
    }
}

Striped64_Storage* Striped64_Storage::expand(Striped64_Storage *existing) {
    return new Striped64_Storage(existing);
}

void Striped64_Storage::disavow() {
    owner_ = false;
}

void Striped64_Storage::disavow_all() {
    owner_ = false;
    clean_created_ = false;
}

inline std::atomic<int64_t>& Striped64_Storage::get(size_t idx) {
    return data_[idx]->data;
}

Striped64_Storage::~Striped64_Storage() {
    int shift = 0;
    size_t slab = 0;
    for (;;) {
        if (owner_ && data_[slab]) {
            delete [] reinterpret_cast<CacheAligned<std::atomic<int64_t>>*>(
                data_[slab]);
        }
        size_t next_slab = (1 << ++shift);
        if (next_slab >= size_) {
            break;
        }
        slab = next_slab;
    }

    if (!owner_ && clean_created_) {
        // We are responsible for the last block of storage
        delete [] reinterpret_cast<CacheAligned<std::atomic<int64_t>>*>(
            data_[slab]);
    }
    delete [] data_;
}

} // ccmetrics namespace
