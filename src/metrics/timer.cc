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

#include "ccmetrics/snapshot.h"
#include "ccmetrics/timer.h"
#include "metrics/histogram.h"
#include "metrics/meter.h"

namespace ccmetrics {

class TimerImpl {
public:
    void update(int64_t duration);

    int64_t count() {
        return histogram_.count();
    }

    double oneMinuteRate() {
        return meter_.oneMinuteRate();
    }

    double fiveMinuteRate() {
        return meter_.fiveMinuteRate();
    }

    double fifteenMinuteRate() {
        return meter_.fifteenMinuteRate();
    }

    Snapshot snapshot() {
        return histogram_.snapshot();
    }
private:
    Histogram histogram_;
    Meter meter_;
};

void TimerImpl::update(int64_t duration) {
    histogram_.update(duration);
    meter_.mark();
}

void Timer::update(int64_t duration) {
    impl_->update(duration);
}

int64_t Timer::count() {
    return impl_->count();
}

double Timer::oneMinuteRate() {
    return impl_->oneMinuteRate();
}

double Timer::fiveMinuteRate() {
    return impl_->fiveMinuteRate();
}

double Timer::fifteenMinuteRate() {
    return impl_->fifteenMinuteRate();
}

Snapshot Timer::snapshot() {
    return impl_->snapshot();
}

Timer::Timer() : impl_(new TimerImpl()) { }
Timer::~Timer() { delete impl_; }

} // ccmetrics namespace
