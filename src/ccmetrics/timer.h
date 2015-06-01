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

#ifndef SRC_CCMETRICS_TIMER_H_
#define SRC_CCMETRICS_TIMER_H_

#include <cinttypes>

#include "ccmetrics/snapshot.h"

namespace ccmetrics {

class TimerImpl;

/**
 * A timer metric that reports aggregate statistics of recorded event durations
 * and throughput estimates.
 */
class Timer {
public:
    Timer();
    ~Timer();

    /** Record an event duration (in ms). */
    void update(int64_t duration);

    /** @return the number of recorded events. */
    int64_t count();

    /** @return the one minute rate, in operations / s. */
    double oneMinuteRate();

    /** @return the five minute rate, in operations / s. */
    double fiveMinuteRate();

    /** @return the fifteen minute rate, in operations / s. */
    double fifteenMinuteRate();

    /** @return a snapshot of the distribution of durations. */
    Snapshot snapshot();
private:
    Timer(Timer const&) = delete;
    Timer& operator=(Timer const&) = delete;
    TimerImpl *impl_;
};

} // ccmetrics namespace

#endif // SRC_CCMETRICS_TIMER_H_
