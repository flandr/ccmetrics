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

#include "metrics/meter_impl.h"

#include <atomic>
#include <cmath>

namespace ccmetrics {

double RateEWMA::rate() {
    tickIfNecessary();
    return rate_;
}

void RateEWMA::update(int64_t val) {
    buffer_.add(val);
    tickIfNecessary();
}

void RateEWMA::tickIfNecessary() {
    auto now = std::chrono::steady_clock::now();
    auto prev = last_tick_.load();
    auto delta_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now - prev.t);

    if (delta_us.count() < kInterval * 1E6) {
        return;
    }

    if (!last_tick_.compare_exchange_strong(prev, CWG1778Hack(now))) {
        return;
    }

    int iters = delta_us.count() / (kInterval * 1E6L);
    for (int i = 0; i < iters; ++i) {
        // TODO: this could be done more efficiently (wrt atomic CAS) within
        // the tick method itself. Consider inlining.
        tick();
    }
}

void RateEWMA::tick() {
    int64_t uncounted = buffer_.value();
    buffer_.reset(); // XXX you just lost some events. Do you care?

    double instant = uncounted / static_cast<double>(kInterval);

    if (init_) {
        double rate = rate_.load();
        while (!rate_.compare_exchange_weak(
                rate, rate + alpha_ * (instant - rate))) { }
    } else {
        rate_ = instant;
        init_ = true;
    }
}

MeterImpl::MeterImpl()
    : oneMinuteRate_(kOneMinuteAlpha),
      fiveMinuteRate_(kFiveMinuteAlpha),
      fifteenMinuteRate_(kFifteenMinuteAlpha) {
}

const double MeterImpl::kOneMinuteAlpha =
    1 - std::exp(-RateEWMA::kInterval / 60.0);
const double MeterImpl::kFiveMinuteAlpha =
    1 - std::exp(-RateEWMA::kInterval / 60.0 / 5.0);
const double MeterImpl::kFifteenMinuteAlpha =
    1 - std::exp(-RateEWMA::kInterval / 60.0 / 15.0);

void MeterImpl::mark(int n) {
    // TODO: moving the "tick if necessary" into the meter yields 1/3 the CAS
    // instructions required by keeping it encapsulated in the rate class.
    // Probably worthwhile.
    oneMinuteRate_.update(n);
    fiveMinuteRate_.update(n);
    fifteenMinuteRate_.update(n);
}

void MeterImpl::mark() {
    mark(1);
}

double MeterImpl::oneMinuteRate() {
    return oneMinuteRate_.rate();
}

double MeterImpl::fiveMinuteRate() {
    return fiveMinuteRate_.rate();
}

double MeterImpl::fifteenMinuteRate() {
    return fifteenMinuteRate_.rate();
}

} // ccmetrics namespace
