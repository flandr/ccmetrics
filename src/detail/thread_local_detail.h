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

#if !defined(_WIN32)
#include <pthread.h>
#endif

#include <cstdint>
#include <mutex>
#include <system_error>
#include <vector>

#include "detail/define_once.h"

namespace ccmetrics {

class SharedStorage;

// Multiplexer of thread-local storage & invasive list element.
class ThreadLocalStorage {
public:
    ThreadLocalStorage() : next_(nullptr), prev_(nullptr) { }

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
        // XXX assert idx >= 1
        if (idx > elements_.size()) {
            elements_.resize(idx);
        }
        // XXX assert elements_[idx].ptr == nullptr
        Element e = {ptr, deleter ? deleter : deletePtr<T>};
        elements_[idx - 1] = e;
    }

    void* get(uint32_t idx) {
        // XXX assert idx >= 1
        if (idx > elements_.size()) {
            elements_.resize(idx);
        }
        return elements_[idx - 1].ptr;
    }

    // Like `get`, but returns the element & doesn't expand. For use
    // when destroying.
    Element* getIfPresent(uint32_t idx) {
        if (idx > elements_.size()) {
            return nullptr;
        }
        return &elements_[idx - 1];
    }

    bool onList() const {
        return next_ != nullptr && prev_ != nullptr;
    }

    void destroyAll() {
        for (auto& element : elements_) {
            element.destroy();
        }
    }
private:
    template<typename T>
    static void deletePtr(void *ptr) {
        delete static_cast<T*>(ptr);
    }

    std::vector<Element> elements_;
    ThreadLocalStorage *next_;
    ThreadLocalStorage *prev_;

    friend class SharedStorage;
};

void unregisterTlsHelper(ThreadLocalStorage *tls);

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
    }

    static void threadExitCleanup(void *ptr) {
        ThreadLocalStorage *tls = reinterpret_cast<ThreadLocalStorage*>(ptr);
        unregisterTlsHelper(tls);
        // The TLS object is inaccessible at this point
        tls->destroyAll();
        delete tls;
    }
private:
    ThreadLocalStorageHandle(ThreadLocalStorageHandle const&) = delete;
    ThreadLocalStorageHandle& operator=(ThreadLocalStorageHandle const&)
        = delete;

    pthread_key_t pthread_key_;
};
#else
// Need to hook thread cleanup in DllMain
static_assert(false, "Unimplemented nightmare town");
#endif

// Global state for tracking all thread-specific storage
class SharedStorage {
public:
    static SharedStorage& singleton() {
        // STATIC_DEFINE_ONCE(SharedStorage, singleton, SharedStorage());
        // XXX fix windows -- needs move constructor for assignment above
        static SharedStorage singleton;
        return singleton;
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
    }
private:
    SharedStorage() : next_id_(0) {
        all_tls_head_.next_ = all_tls_head_.prev_ = &all_tls_head_;
    }

    void addThread(ThreadLocalStorage *tls) {
        // XXX assert !tls->onList()
        tls->next_ = &all_tls_head_;
        tls->prev_ = all_tls_head_.prev_;
        tls->prev_->next_ = tls;
        all_tls_head_.prev_ = tls;
    }

    void removeThread(ThreadLocalStorage *tls) {
        // XXX assert tls->onList()
        tls->next_->prev_ = tls->prev_;
        tls->prev_->next_ = tls->next_;
        tls->next_ = tls->prev_ = nullptr;
    }

    // Accessor for cross-platform local storage
    ThreadLocalStorageHandle tls_handle_;

    // Global storage
    std::mutex mutex_;
    std::vector<uint32_t> free_list_;
    uint32_t next_id_;

    // Threads that access TLS are registered here
    ThreadLocalStorage all_tls_head_;
};

inline void unregisterTlsHelper(ThreadLocalStorage *tls) {
    SharedStorage::forget(tls);
}

} // ccmetrics namespace

#endif // SRC_DETAIL_THREAD_LOCAL_DETAIL_H_H_
