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

#ifndef SRC_DETAIL_DEFINE_ONCE_H_
#define SRC_DETAIL_DEFINE_ONCE_H_

namespace ccmetrics {

#if !defined(_WIN32)
#define STATIC_DEFINE_ONCE(type, var, stmt) static type var = stmt
#else
// Define a static local variable once, safely, for MSVC
//
// This macro is necessary because MSVC pre-VS14 doesn't
// properly implement C++11 static local initialization.
// It is equivalent to writing something like
//
//     static type var = stmt;
//
// in a compliant compiler (e.g. GCC since who knows when)

// States for lock checking
enum { uninitialized = 0, initializing, initialized };

// Preprocessor hackery for anonymous variables
#define PASTE_IMPL(x, y) x ## y
#define PASTE(x, y) PASTE_IMPL(x, y)
#define ANON_VAR(var) PASTE(var, __LINE__)

#define STATIC_DEFINE_ONCE(type, var, stmt)                     \
    static type var;                                            \
    static int ANON_VAR(state);                                 \
    bool ANON_VAR(cont) = true;                                 \
    while (ANON_VAR(cont)) {                                    \
        switch (InterlockedCompareExchange(&ANON_VAR(state),    \
                initializing, uninitialized)) {                 \
        case uninitialized:                                     \
            var = stmt;                                         \
            InterlockedExchange(&ANON_VAR(state), initialized); \
            ANON_VAR(cont) = false;                             \
            break;                                              \
        case initializing:                                      \
            continue;                                           \
        case initialized:                                       \
            ANON_VAR(cont) = false;                             \
            break;                                              \
        }                                                       \
    } do { } while (0)
#endif

} // ccmetrics namespace

#endif // endif SRC_DETAIL_DEFINE_ONCE_H_
