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

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "wte/event_base.h"
#include "wte/porting.h"
#include "wte/timeout.h"
#include "reporting/util.h"

namespace ccmetrics {

class PeriodicReporterImpl {
public:
    PeriodicReporterImpl(PeriodicReporter *reporter)
        : reporter_(reporter), base_(wte::mkEventBase()), timeout_(this),
          running_(false) { }
    ~PeriodicReporterImpl() {
        delete base_;
    }

    class ReporterTimeout final : public wte::Timeout {
    public:
        explicit ReporterTimeout(PeriodicReporterImpl *reporterImpl)
            : reporterImpl_(reporterImpl) { }

        void expired() NOEXCEPT {
            // Re-register beofre reporting to maintain consistent period
            reporterImpl_->base_->registerTimeout(this, &tv_);

            // Report
            reporterImpl_->reporter_->report();
        }

        void setTimeoutAndSchedule(std::chrono::milliseconds const& ms) {
            tv_.tv_sec = ms.count() / 1000;
            tv_.tv_usec = (ms.count() % 1000) * 1000;
            reporterImpl_->base_->registerTimeout(this, &tv_);
        }
    private:
        PeriodicReporterImpl *reporterImpl_ = nullptr;
        struct timeval tv_;
    };

    bool running() {
        return running_;
    }

    void stop() {
        base_->runOnEventLoopAndWait([this]() -> void {
                base_->unregisterTimeout(&timeout_);
            });
        running_ = false;
        if (loop_.joinable()) {
            loop_.join();
        }
    }

    void start(std::chrono::milliseconds const& period) {
        assert(!running_);
        running_ = true;
        timeout_.setTimeoutAndSchedule(period);
        // Start the event loop
        loop_ = std::thread([this]() -> void {
                base_->loop(wte::EventBase::LoopMode::UNTIL_EMPTY);
            });
        // Wait for the loop to come up
        base_->runOnEventLoopAndWait([]() -> void { }, /*defer=*/ true);
    }
private:
    PeriodicReporter *reporter_;
    wte::EventBase *base_;
    ReporterTimeout timeout_;

    bool running_;
    std::thread loop_;

    friend wte::EventBase* getReporterBase(PeriodicReporterImpl *);
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

wte::EventBase* getReporterBase(PeriodicReporterImpl *reporter) {
    return reporter->base_;
}

} // ccmetrics namespace
