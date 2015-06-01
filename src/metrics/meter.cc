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

#include "metrics/meter.h"

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
    auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - prev.t);

    if (delta_ms.count() < kInterval * 1000) {
        return;
    }

    if (!last_tick_.compare_exchange_strong(prev, CWG1778Hack(now))) {
        return;
    }

    int iters = delta_ms.count() / (kInterval * 1000);
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

Meter::Meter()
    : oneMinuteRate_(kOneMinuteAlpha),
      fiveMinuteRate_(kFiveMinuteAlpha),
      fifteenMinuteRate_(kFifteenMinuteAlpha) {
}

const double Meter::kOneMinuteAlpha =
    1 - std::exp(-RateEWMA::kInterval / 60.0);
const double Meter::kFiveMinuteAlpha =
    1 - std::exp(-RateEWMA::kInterval / 60.0 / 5.0);
const double Meter::kFifteenMinuteAlpha =
    1 - std::exp(-RateEWMA::kInterval / 60.0 / 15.0);

void Meter::mark() {
    // TODO: moving the "tick if necessary" into the meter yields 1/3 the CAS
    // instructions required by keeping it encapsulated in the rate class.
    // Probably worthwhile.
    oneMinuteRate_.update(1);
    fiveMinuteRate_.update(1);
    fifteenMinuteRate_.update(1);
}

double Meter::oneMinuteRate() {
    return oneMinuteRate_.rate();
}

double Meter::fiveMinuteRate() {
    return fiveMinuteRate_.rate();
}

double Meter::fifteenMinuteRate() {
    return fifteenMinuteRate_.rate();
}

} // ccmetrics namespace
