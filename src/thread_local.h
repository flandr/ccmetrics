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
 *
 * [1] http://www.boost.org/doc/libs/1_58_0/doc/html/thread/thread_local_storage.html
 * [2] https://github.com/facebook/folly
 */
template<typename T>
class ThreadLocalPointer {
public:
#if !defined(_WIN32)
    ThreadLocalPointer() : id_(SharedStorage::create()) { }
#else
    // There is a bug in VS 2013 that deadlocks the process when linking to
    // a DLL that has static classes with initializers that aquire std::mutex.
    // This is an implementation detail, unfortunately, that leaks up to
    // this level; for Windows, we delay acquisition of the TLS slot id until
    // first use, incurring an additional conditional evaluation on every
    // access but allowing this library to be dynamically loaded.
    // See http://connect.microsoft.com/VisualStudio/feedbackdetail/view/900741/deadlock-exception-using-std-mutex-in-static-class-instance-in-dll
    ThreadLocalPointer() : id_(0) { }
#endif

    ~ThreadLocalPointer() {
#if defined(_WIN32)
        if (!id_) {
            return;
        }
#endif
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

    void reset(T* ptr, void(*deleter)(void*) = nullptr) {
#if defined(_WIN32)
        if (!id_) {
            id_ = SharedStorage::create();
        }
#endif
        SharedStorage::set(id_, ptr, deleter);
    }

    T* get() const {
#if defined(_WIN32)
        if (!id_) {
            const_cast<uint32_t&>(id_) = SharedStorage::create();
        }
#endif
        return static_cast<T*>(SharedStorage::get(id_));
    }
protected:
    // Visible for testing
    uint32_t id() const { return id_; }
private:
    uint32_t id_; // Element id
};

template<typename T>
struct DefaultNew {
    T* operator()(void) const {
        return new T();
    }
};

template<typename T, typename NewFunctor = DefaultNew<T>>
class ThreadLocal {
public:
    ThreadLocal() : new_functor_(NewFunctor()), deleter_(nullptr) { }
    explicit ThreadLocal(NewFunctor && nf)
        : new_functor_(nf), deleter_(nullptr) { }
    ThreadLocal(NewFunctor && nf, void(*deleter)(void*))
        : new_functor_(nf), deleter_(deleter) { }

    T* operator->() const {
       return get();
    }

    T& operator*() const {
        return *get();
    }

#if !defined(_WIN32)
    ThreadLocal(ThreadLocal &&) = default;
    ThreadLocal& operator=(ThreadLocal &&) = default;
#endif
    ThreadLocal(ThreadLocal const&) = delete;
    ThreadLocal& operator=(ThreadLocal const&) = delete;
private:
    T* get() const {
        T* ret = ptr_.get();
        if (ret) {
            return ret;
        }

        // Allocate on first use
        ret = new_functor_();
        ptr_.reset(ret, deleter_);
        return ret;
    }

    // Allow stateful type construction
    NewFunctor new_functor_;

    // Allow custom deletion
    void (*deleter_)(void*);

    // Mutable for on-use allocation
    mutable ThreadLocalPointer<T> ptr_;
};

} // ccmetrics namespace

#endif // SRC_THREAD_LOCAL_H_
