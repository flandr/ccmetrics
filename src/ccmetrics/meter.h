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

#ifndef SRC_CCMETRICS_METER_H_
#define SRC_CCMETRICS_METER_H_

#include "ccmetrics/porting.h"

namespace ccmetrics {

class MeterImpl;

/** A meter that provides rate estimates. */
class CCMETRICS_SYM Meter {
public:
    Meter();
    ~Meter();

    /** Record an event. */
    void mark();

    /** Record `n` events. */
    void mark(int n);

    /** @return the one minute rate. */
    double oneMinuteRate();

    /** @return the five minute rate. */
    double fiveMinuteRate();

    /** @return the fifteen minute rate. */
    double fifteenMinuteRate();
private:
    Meter(Meter const&) = delete;
    Meter& operator=(Meter const&) = delete;
    MeterImpl *impl_;
};

} // ccmetrics namespace

#endif // SRC_CCMETRICS_METER_H_
