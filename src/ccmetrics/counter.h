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

#ifndef SRC_CCMETRICS_COUNTER_H_
#define SRC_CCMETRICS_COUNTER_H_

#include <cinttypes>

#include "ccmetrics/porting.h"

namespace ccmetrics {

class CounterImpl;

/** An integral counter metric. */
class CCMETRICS_SYM Counter {
public:
    Counter();
    ~Counter();

    /** Decrement counter by one. */
    void dec();

    /* Increment counter by one. */
    void inc();

    /** Set value += delta. */
    void update(int64_t delta);

    /** @return the counter value. */
    int64_t value();
private:
    Counter(Counter const&) = delete;
    Counter& operator=(Counter const&) = delete;
    CounterImpl *impl_;
};

} // ccmetrics namespace

#endif // SRC_CCMETRICS_COUNTER_H_
