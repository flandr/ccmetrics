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
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SRC_CACHE_ALIGNED_H_
#define SRC_CACHE_ALIGNED_H_

#include <stdlib.h>

#include <cstdlib>
#include <new>
#include <utility>

namespace ccmetrics {

// XXX orly?
#define CACHE_LINE_SIZE 64

#ifdef _WIN32
#define CACHE_ALIGNED __declspec(align(CACHE_LINE_SIZE))
#else
#define CACHE_ALIGNED __attribute__((aligned(CACHE_LINE_SIZE)))
#endif

static inline void* allocateAligned(std::size_t size, std::size_t alignment) {
    void *p = nullptr;

#ifdef _WIN32
    p = _aligned_malloc(size, alignment);
#else
    posix_memalign(&p, alignment, size);
#endif
    if (!p) {
        throw std::bad_alloc();
    }
    return p;
}

static inline void deallocateAligned(void *p) {
#ifdef _WIN32
    _aligned_free(p);
#else
    free(p);
#endif
}

/** A cache alignement & padding wrapper for arbitrary types. */
template<typename T>
struct CACHE_ALIGNED CacheAligned {
    T data;
    char pad[CACHE_LINE_SIZE > sizeof(T) ? CACHE_LINE_SIZE - sizeof(T) : 1];

    template<typename... U>
    CacheAligned(U&&... u) : data(std::forward<U>(u)...) { }

    void* operator new(std::size_t size) {
        return allocateAligned(size, CACHE_LINE_SIZE);
    }

    void* operator new[](std::size_t size) {
        return allocateAligned(size, CACHE_LINE_SIZE);
    }

    void operator delete(void *p) {
        deallocateAligned(p);
    }

    void operator delete[](void *p) {
        deallocateAligned(p);
    }
};

/**
 * Allocate a cache aligned object.
 *
 * Such pointers *must* be released by calls to `freeAligned`.
 */
template<typename T, typename... U>
T* mkAligned(U&&... u) {
    return new CacheAligned<T>(std::forward<U>(u)...);
}

/** Free a pointer returned from `mkAligned`. */
template<typename T>
void freeAlgined(T* t) {
    delete reinterpret_cast<CacheAligned<T>*>(t);
}

} // ccmetrics namespace

#endif // SRC_CACHE_ALIGNED_H_
