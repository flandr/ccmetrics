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

#include "ccmetrics/reporting/periodic_reporter.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace ccmetrics {

class PeriodicReporterImpl {
public:
    PeriodicReporterImpl(PeriodicReporter *reporter)
        : reporter_(reporter), running_(false), stop_(false) { }

    bool running() {
        return running_;
    }

    void stop() {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }
        stop_ = true;
        cv_.notify_all();
        cv_.wait(lock, [this]() { return !running_; });
        worker_.join();
    }

    void start(std::chrono::milliseconds const& period) {
        stop_ = false;
        running_ = true;
        worker_ = std::thread([this, period]() -> void {
                for (;;) {
                    reporter_->report();

                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait_for(lock, period, [this]() { return stop_; });
                    if (stop_) {
                        running_ = false;
                        cv_.notify_all();
                        break;
                    }
                }
            });
    }
private:
    PeriodicReporter *reporter_;
    std::thread worker_;

    std::mutex mutex_;
    std::condition_variable cv_;
    bool running_;
    bool stop_;
};

PeriodicReporter::PeriodicReporter() : impl_(new PeriodicReporterImpl(this)) { }
PeriodicReporter::~PeriodicReporter() {
    stop();
    delete impl_;
}

void PeriodicReporter::doStart(std::chrono::milliseconds const& period) {
    impl_->start(period);
}

void PeriodicReporter::stop() {
    impl_->stop();
}

void PeriodicReporter::Deleter::operator()(PeriodicReporter *reporter) {
    delete reporter;
}

} // ccmetrics namespace
