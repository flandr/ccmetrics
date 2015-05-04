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

#ifndef SRC_THREAD_LOCAL_H_
#define SRC_THREAD_LOCAL_H_

#include <cstdint>

#include "detail/thread_local_detail.h"

namespace ccmetrics {

/**
 * Cross-platform wrapper for thread-local storage.
 *
 * Like Boost [1] and Folly [2] implementations, this implementation
 * multiplexes multiple values onto the same underlying system-specific
 * thread local value. This plays well with use of pthread_key_t (limited
 * number of keys available) as well as __declspec(thread) on Windows
 * (POD types only). Like the Folly implementation, uses an array and
 * free list for handle storage rather than an ordered map (as Boost does),
 * allowing O(1) access (new thread local creations should be rare).
 *
 * The tagged allocation reservoir in the Folly implementation may be useful
 * for avoiding contention if the rare-creation assumption turns out not
 * to be true, or if serialization on thread creation & deletion is somehow
 * concerning (hint: it's not), or if I add iteration.
 */
template<typename T>
class ThreadLocalPointer {
public:
    ThreadLocalPointer() : id_(SharedStorage::create()) { }
    ~ThreadLocalPointer() {
        SharedStorage::destroy(id_);
    }

    ThreadLocalPointer(ThreadLocalPointer && o) : id_(o.id_) {
        o.id_ = 0;
    }

    T* operator->() const {
        return get();
    }

    T& operator*() const {
        return *get();
    }

    void reset(T* ptr) {
        SharedStorage::set(id_, ptr);
    }

    T* get() const {
        return static_cast<T*>(SharedStorage::get(id_));
    }
protected:
    // Visible for testing
    uint32_t id() const { return id_; }
private:
    uint32_t id_; // Element id
};

template<typename T>
class ThreadLocal {
public:
    ThreadLocal() { }

    T* operator->() const {
       return get();
    }

    T& operator*() const {
        return *get();
    }

    ThreadLocal(ThreadLocal &&) = default;
    ThreadLocal& operator=(ThreadLocal &&) = default;
    ThreadLocal(ThreadLocal const&) = delete;
    ThreadLocal& operator=(ThreadLocal const&) = delete;
private:
    T* get() const {
        T* ret = ptr_.get();
        if (ret) {
            return ret;
        }

        // Allocate on first use
        ret = new T();
        ptr_.reset(ret);
        return ret;
    }

    // Mutable for on-use allocation
    mutable ThreadLocalPointer<T> ptr_;
};

} // ccmetrics namespace

#endif // SRC_THREAD_LOCAL_H_
