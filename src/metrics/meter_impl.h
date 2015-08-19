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

#ifndef SRC_METRICS_METER_H_
#define SRC_METRICS_METER_H_

#include <cinttypes>

#include "metrics/cwg1778hack.h"
#include "striped_int64.h"

namespace ccmetrics {

namespace test { class RateEWMATest; }

/**
 * Exponentially weighted moving average of a rate.
 *
 * This class is an average over a _time window_, not over the sample count.
 * It buffers updates and applys them when ticked. In the case that no tick-
 * invoking method is called for > 1 tick period, it repeatedly invokes the
 * tick method to decay the rate.
 */
class RateEWMA {
public:
    explicit RateEWMA(double alpha) : alpha_(alpha), rate_(0.0), init_(false),
        last_tick_(CWG1778Hack(std::chrono::steady_clock::now())) { }

    /** Update the average with a new value. */
    void update(int64_t val);

    /** @return the rate. */
    double rate();

    static constexpr const int32_t kInterval = 5; // seconds
private:
    /** Tick the time forward if necessary. */
    void tickIfNecessary();

    /** Tick the time forward one interval. */
    void tick();

    double alpha_;
    // Buffered updates
    Striped64 buffer_;
    std::atomic<double> rate_;
    std::atomic<bool> init_;
    std::atomic<CWG1778Hack> last_tick_;

    friend class test::RateEWMATest;
};

/**
 * Meter that tracks exponentially weighted moving average for one, five, and
 * fifteen minute rates. Thus, basicallly UNIX load average.
 */
class MeterImpl {
public:
    MeterImpl();

    /** Mark that an event occurred. */
    void mark();

    /** Mark that `n` events occurred. */
    void mark(int n);

    /** @return one minute rate. */
    double oneMinuteRate();

    /** @return five minute rate. */
    double fiveMinuteRate();

    /** @return fifteen minute rate. */
    double fifteenMinuteRate();

    // visible for testing
    static const double kOneMinuteAlpha;
    static const double kFiveMinuteAlpha;
    static const double kFifteenMinuteAlpha;
private:

    RateEWMA oneMinuteRate_;
    RateEWMA fiveMinuteRate_;
    RateEWMA fifteenMinuteRate_;
};

} // ccmetrics namespace

#endif // SRC_METRICS_METER_H_
