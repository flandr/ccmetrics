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

#ifndef SRC_METRICS_COUNTER_IMPL_H_
#define SRC_METRICS_COUNTER_IMPL_H_

#include "striped_int64.h"

namespace ccmetrics {

/*
 * Implementation of a counter that uses a sharded reservior to implement
 * concurrent value updates w/ reduced contention. Concurrent read and
 * modification of this counter can yield inconsitent results; this is
 * an acceptable trade-off for use as a counter metric.
 */
class CounterImpl {
public:
    void dec() { update(-1); }
    void inc() { update(1); }
    void update(int64_t delta) {
        value_.add(delta);
    }
    int64_t value() {
        return value_.value();
    }
private:
    Striped64 value_;
};

} // ccmetrics namespace

#endif // SRC_METRICS_COUNTER_IMPL_H_
