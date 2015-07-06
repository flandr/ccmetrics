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

#ifndef SRC_DETAIL_THREAD_LOCAL_DETAIL_H_H_
#define SRC_DETAIL_THREAD_LOCAL_DETAIL_H_H_

#include <string.h>

#if !defined(_WIN32)
#include <pthread.h>
#endif

#include <cassert>
#include <cstdint>
#include <mutex>
#include <system_error>
#include <vector>

#include "ccmetrics/detail/define_once.h"

namespace ccmetrics {

class SharedStorage;

// Multiplexer of thread-local storage & invasive list element.
class ThreadLocalStorage {
public:
    constexpr ThreadLocalStorage() : elements_(nullptr), n_elements_(0),
        next_(nullptr), prev_(nullptr) { }

    struct Element {
        void *ptr;
        void (*deleter)(void *);

        /** Destroy the element, returning whether it existed to begin with. */
        bool destroy() {
            if (!ptr) {
                return false;
            }
            deleter(ptr);
            ptr = nullptr;
            deleter = nullptr;
            return true;
        }
    };

    template<typename T>
    void set(uint32_t idx, T *ptr, void(*deleter)(void *)) {
        assert(idx >= 1);

        if (idx > n_elements_) {
            resize(idx);
        }

        assert(elements_[idx - 1].ptr == nullptr);

        Element e = {ptr, deleter ? deleter : deletePtr<T>};
        elements_[idx - 1] = e;
    }

    void* get(uint32_t idx) {
        assert(idx >= 1);

        if (idx > n_elements_) {
            resize(idx);
        }
        return elements_[idx - 1].ptr;
    }

    // Like `get`, but returns the element & doesn't expand. For use
    // when destroying.
    Element* getIfPresent(uint32_t idx) {
        if (idx > n_elements_) {
            return nullptr;
        }
        return &elements_[idx - 1];
    }

    bool onList() const {
        return next_ != nullptr && prev_ != nullptr;
    }

    void destroyAll() {
        for (size_t i = 0; i < n_elements_; ++i) {
            elements_[i].destroy();
        }
        delete [] elements_;
        elements_ = nullptr;
        n_elements_ = 0;
    }

    size_t capacity() const { return n_elements_; }
private:
    template<typename T>
    static void deletePtr(void *ptr) {
        delete static_cast<T*>(ptr);
    }

    void resize(size_t size) {
        assert(size > n_elements_);

        // Use an expansion factor < 2, which allows for eventual use of
        // previously allocated blocks by the allocator; see some discussion
        // at http://stackoverflow.com/questions/5232198/about-vectors-growth.
        size_t new_size = size * 1.5;

        Element *next = new Element[new_size];
        memset(next, 0, new_size * sizeof(Element));
        if (n_elements_ > 0) {
            memcpy(next, elements_, n_elements_ * sizeof(Element));
        }
        delete [] elements_;
        elements_ = next;
        n_elements_ = new_size;
    }

    Element *elements_;
    size_t n_elements_;
    ThreadLocalStorage *next_;
    ThreadLocalStorage *prev_;

    friend class SharedStorage;
};

void unregisterTlsHelper(ThreadLocalStorage *tls);

class ThreadLocalStorageHandle;

#if defined(_WIN32)
#define TLS_SPECIFIER __declspec(thread)
#elif defined(__APPLE__)
// Although Apple supports __thread in OS X > 10.6, it interacts badly with
// pthread_key_create w/ a cleanup callback; in particular, the TLS is no longer
// available when the callback is invoked, leading to the cleanup code running
// over garbage memory. We'll stick to using pthread_{set,get}specific here.
#else
#define TLS_SPECIFIER __thread
#endif

#if !defined(_WIN32)
class ThreadLocalStorageHandle {
public:
    ThreadLocalStorageHandle() {
        int rc = pthread_key_create(&pthread_key_, threadExitCleanup);
        if (rc) {
            throw std::system_error(rc, std::system_category(),
                "pthread_key_create failed");
        }
    }

    ~ThreadLocalStorageHandle() {
        pthread_key_delete(pthread_key_);
    }

    ThreadLocalStorage* get() {
#if defined(TLS_SPECIFIER)
        if (tls_.capacity() == 0) {
            int rc = pthread_setspecific(pthread_key_, &tls_);
            if (rc) {
                throw std::system_error(rc, std::system_category(),
                    "pthread_setspecific failed");
            }
            // Reserve one slot to avoid subsequent initialization
            // if the caller doesn't issue get/set. This isn't strictly
            // necessary, but we'd expect just the single setspecific call.
            tls_.get(1);
        }
        return &tls_;
#else
        ThreadLocalStorage* entry = reinterpret_cast<ThreadLocalStorage*>(
            pthread_getspecific(pthread_key_));
        if (entry) {
            return entry;
        }
        entry = new ThreadLocalStorage();

        int rc = pthread_setspecific(pthread_key_, entry);
        if (rc) {
            delete entry;
            throw std::system_error(rc, std::system_category(),
                "pthread_setspecific failed");
        }

        return entry;
#endif
    }

    static void threadExitCleanup(void *ptr) {
        ThreadLocalStorage *tls = reinterpret_cast<ThreadLocalStorage*>(ptr);
        unregisterTlsHelper(tls);
        // The TLS object is inaccessible at this point
        tls->destroyAll();
#if !defined(TLS_SPECIFIER)
        delete tls;
#endif
    }
private:
    ThreadLocalStorageHandle(ThreadLocalStorageHandle const&) = delete;
    ThreadLocalStorageHandle& operator=(ThreadLocalStorageHandle const&)
        = delete;

#if defined(TLS_SPECIFIER)
    static TLS_SPECIFIER ThreadLocalStorage tls_;
#endif

    pthread_key_t pthread_key_;
};
#else
class ThreadLocalStorageHandle {
public:
    ThreadLocalStorageHandle() { }
    ~ThreadLocalStorageHandle() { }

    ThreadLocalStorage* get() {
        return &tls_;
    }

    static void threadExitCleanup() {
        unregisterTlsHelper(&tls_);
        // The TLS object is inaccessible at this point
        tls_.destroyAll();
    }
private:
    // Cleanup is handled by DllMain
    static TLS_SPECIFIER ThreadLocalStorage tls_;
};
#endif

// Helper class to implement a kind of zero-or-one reference counting.
// This class informs the wrapped object when it has destructed, allowing
// it to enter a teardown mode while avoiding static destruction order issues
// if handles are held by other static objects. An alternative appraoch to
// this problem is to use std::shared_ptr, which can incur substantially more
// reference counting overhead, depending on the object implementation.
// See SharedStorage.
template<typename T>
class StaticDestructionGuard {
public:
    StaticDestructionGuard(T *t) : t_(t) { }
    ~StaticDestructionGuard() {
        t_->staticallyDestructed();
    }
    T& get() { return t_; }
private:
    T* t_;
};

// Global state for tracking all thread-specific storage
class SharedStorage {
public:
    static SharedStorage& singleton() {
        // XXX Needs a fix for pre-VC14 Windows (use STATIC_DEFINE_ONCE),
        // though implementation details of this library ensure that this
        // is invoked in a single-threaded context on first use.
        static SharedStorage *singleton = new SharedStorage();
        static StaticDestructionGuard<SharedStorage> guard(singleton);
        return *singleton;
    }

    /** @return a key into the thread-specific storage. */
    static uint32_t create() {
        auto& ss = singleton();
        std::lock_guard<std::mutex> lock(ss.mutex_);
        if (ss.free_list_.empty()) {
            return ++ss.next_id_;
        }
        uint32_t ret = ss.free_list_.back();
        ss.free_list_.pop_back();
        return ret;
    }

    /** @return the pointer, or nullptr if no such element is registered. */
    static void* get(uint32_t id) {
        auto& ss = singleton();
        auto* tls = singleton().tls_handle_.get();

        if (!tls->onList()) {
            // First access by this thread
            std::lock_guard<std::mutex> lock(ss.mutex_);
            ss.addThread(tls);
        }

        return tls->get(id);
    }

    /** Set the value stored by an id, replacing if it exists. */
    template<typename T>
    static void set(uint32_t id, T* ptr, void(*deleter)(void *)) {
        auto& ss = singleton();
        auto* tls = singleton().tls_handle_.get();

        if (!tls->onList()) {
            // First access by this thread
            std::lock_guard<std::mutex> lock(ss.mutex_);
            ss.addThread(tls);
        }

        tls->set(id, ptr, deleter);
    }

    static void destroy(uint32_t id) {
        auto& ss = singleton();

        // Need to iterate all of the registered threads, finding any
        // that have a value for the matching id
        std::lock_guard<std::mutex> lock(ss.mutex_);
        ThreadLocalStorage *cur = ss.all_tls_head_.next_;
        while (cur != &ss.all_tls_head_) {
            auto* entry = cur->getIfPresent(id);
            if (entry) {
                entry->destroy();
            }
            cur = cur->next_;
        }
        ss.free_list_.push_back(id);
    }

    static void forget(ThreadLocalStorage *tls) {
        auto& ss = singleton();
        std::lock_guard<std::mutex> lock(ss.mutex_);
        ss.removeThread(tls);
        if (ss.destructed_ && ss.threadsEmpty()) {
            // Last reference; we're the cleanup crew
            delete &ss;
        }
    }

    void staticallyDestructed() {
        destructed_ = true;
    }
private:
    SharedStorage() : next_id_(0), destructed_(false) {
        all_tls_head_.next_ = all_tls_head_.prev_ = &all_tls_head_;
    }

    void addThread(ThreadLocalStorage *tls) {
        assert(!tls->onList());

        tls->next_ = &all_tls_head_;
        tls->prev_ = all_tls_head_.prev_;
        tls->prev_->next_ = tls;
        all_tls_head_.prev_ = tls;
    }

    void removeThread(ThreadLocalStorage *tls) {
        assert(tls->onList());

        tls->next_->prev_ = tls->prev_;
        tls->prev_->next_ = tls->next_;
        tls->next_ = tls->prev_ = nullptr;
    }

    bool threadsEmpty() {
        return all_tls_head_.next_ == &all_tls_head_;
    }

    // Accessor for cross-platform local storage
    ThreadLocalStorageHandle tls_handle_;

    // Global storage
    std::mutex mutex_;
    std::vector<uint32_t> free_list_;
    uint32_t next_id_;
    bool destructed_;

    // Threads that access TLS are registered here
    ThreadLocalStorage all_tls_head_;
};

inline void unregisterTlsHelper(ThreadLocalStorage *tls) {
    SharedStorage::forget(tls);
}

} // ccmetrics namespace

#endif // SRC_DETAIL_THREAD_LOCAL_DETAIL_H_H_
