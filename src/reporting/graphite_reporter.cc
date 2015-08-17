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

#include <cinttypes>
#include <ctime>

#include "format.h"

#include "ccmetrics/reporting/periodic_reporter.h"
#include "reporting/util.h"
#include "wte/buffer.h"
#include "wte/event_base.h"
#include "wte/stream.h"

namespace ccmetrics {

class GraphiteReporter final : public PeriodicReporter {
public:
    explicit GraphiteReporter(MetricRegistry *registry,
        std::string const& graphite_ip, int16_t graphite_port);
    ~GraphiteReporter();
    void report() NOEXCEPT;

    // Throws
    void connect();
private:
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED
    };

    class WriteCallback final : public wte::Stream::WriteCallback {
    public:
        explicit WriteCallback(GraphiteReporter *reporter)
            : reporter_(reporter) { }
        void complete(wte::Stream *stream);
        void error(std::runtime_error const& error);
    private:
        GraphiteReporter *reporter_;
    };

    class ConnectCallback final : public wte::Stream::ConnectCallback {
    public:
        explicit ConnectCallback(GraphiteReporter *reporter)
            : reporter_(reporter) { }
        void complete();
        void error(std::runtime_error const& error);
    private:
        GraphiteReporter *reporter_;
    };

    void writeCounter(wte::Buffer *buffer, std::string const& name,
        Counter *counter, int64_t timestamp);
    void writeTimer(wte::Buffer *buffer, std::string const& name,
        Timer *timer, int64_t timestamp);

    std::string prefix(std::string const& name, std::string const& val) {
        return name + "." + val;
    }

    WriteCallback wcb_;
    ConnectCallback ccb_;
    State state_;

    MetricRegistry *registry_;
    std::string host_ip_;
    int16_t port_;
    std::unique_ptr<wte::Stream, wte::Stream::Deleter> stream_;
};

void GraphiteReporter::ConnectCallback::complete() {
    assert(reporter_->state_ == State::CONNECTING);
    reporter_->state_ = State::CONNECTED;
    // Now invoke the report call that triggered this connect
    reporter_->report();
}

void GraphiteReporter::ConnectCallback::error(std::runtime_error const& e) {
    // TODO: logging?
    assert(reporter_->state_ == State::CONNECTING);
    reporter_->state_ = State::DISCONNECTED;
}

void GraphiteReporter::WriteCallback::complete(wte::Stream *) { }

void GraphiteReporter::WriteCallback::error(std::runtime_error const& e) {
    assert(reporter_->state_ == State::CONNECTED);
    reporter_->stream_->close();
    reporter_->state_ = State::DISCONNECTED;
}

GraphiteReporter::GraphiteReporter(MetricRegistry *registry,
        std::string const& ip, int16_t port)
    : wcb_(this), ccb_(this), state_(State::DISCONNECTED), registry_(registry),
      host_ip_(ip), port_(port) {
}

GraphiteReporter::~GraphiteReporter() {
    // XXX what if it's connecting?
    if (stream_) {
        stream_->close();
    }
}

void GraphiteReporter::connect() {
    assert(state_ == State::DISCONNECTED);

    state_ = State::CONNECTING;
    auto base = getReporterBase(impl_);
    stream_ = wte::Stream::create(base);
    stream_->connect(host_ip_, port_, &ccb_);
}

void GraphiteReporter::writeCounter(wte::Buffer *buffer,
        std::string const& name, Counter *counter, int64_t timestamp) {
    buffer->append(fmt::format("{} {} {}\n", prefix(name, "count"),
        counter->value(), timestamp));
}

void GraphiteReporter::writeTimer(wte::Buffer *buffer,
        std::string const& name, Timer *timer, int64_t ts) {
    std::string f;

    auto snap = timer->snapshot();

    buffer->append(fmt::format("{} {} {}\n",
        prefix(name, "count"), timer->count(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "m1_rate"), timer->oneMinuteRate(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "m5_rate"), timer->fiveMinuteRate(), ts));
    buffer->append(fmt::format("{} {:2.2f} {} \n",
        prefix(name, "m15_rate"), timer->fifteenMinuteRate(), ts));

    buffer->append(fmt::format("{} {} {}\n",
        prefix(name, "min"), snap.min(), ts));
    buffer->append(fmt::format("{} {} {}\n",
        prefix(name, "max"), snap.max(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "mean"), snap.mean(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "stdev"), snap.stdev(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "median"), snap.median(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "p75"), snap.get75tile(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "p95"), snap.get95tile(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "p99"), snap.get99tile(), ts));
    buffer->append(fmt::format("{} {:2.2f} {}\n",
        prefix(name, "p999"), snap.get999tile(), ts));
}

void GraphiteReporter::report() NOEXCEPT {
    switch (state_) {
    case State::DISCONNECTED:
        connect();
        return;
    case State::CONNECTING:
        // Probably this means an overly aggressive reporting period.
        return;
    default:
        ;
    }

    auto writebuf = wte::Buffer::create();
    int64_t unix_timestamp = std::chrono::seconds(std::time(NULL)).count();

    auto counters = registry_->counters();
    for (auto& entry : counters) {
        writeCounter(writebuf.get(), entry.first, entry.second, unix_timestamp);
    }

    auto timers = registry_->timers();
    for (auto& entry : timers) {
        writeTimer(writebuf.get(), entry.first, entry.second,  unix_timestamp);
    }

    // XXX ew. Fix this in wte.
    stream_->write(writebuf.get(), &wcb_);
}

std::unique_ptr<PeriodicReporter, PeriodicReporter::Deleter>
mkGraphiteReporter(MetricRegistry *registry, std::string const& ip,
        int16_t port) {
    return std::unique_ptr<PeriodicReporter, PeriodicReporter::Deleter>(
        new GraphiteReporter(registry, ip, port), PeriodicReporter::Deleter());
}

} // ccmetrics namespace
