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

#include <stdio.h>
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>

#include <cassert>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "ccmetrics/metric_registry.h"
#include "ccmetrics/reporting/periodic_reporter.h"

#if defined(_WIN32)
#define noexcept
#endif

namespace ccmetrics {

class ConsoleReporter : public PeriodicReporter {
public:
    explicit ConsoleReporter(MetricRegistry *registry) : registry_(registry) { }
    void report() noexcept;

    static const int kKeyWidth = 20;
private:
    void printWithBanner(std::string const& str, char sym);
    void printCounter(Counter *ctr);
    void printTimer(Timer *timer);

    MetricRegistry *registry_;
};

template<typename T>
void printFormatted(const char *lhs, const char *equality, T value,
        const char *rhs);

template<>
void printFormatted(const char *lhs, const char *equality, double value,
        const char *rhs) {
    printf("%*s %s %2.2f %s\n", ConsoleReporter::kKeyWidth, lhs,
        equality, value, rhs);
}

template<>
void printFormatted(const char *lhs, const char *equality, int64_t value,
        const char *rhs) {
    printf("%*s %s %" PRId64" %s\n", ConsoleReporter::kKeyWidth, lhs,
        equality, value, rhs);
}

static std::string formatNow() {
    std::time_t t = std::time(nullptr);
#if defined(__APPLE__) || defined(_WIN32)
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y-%m-%d %X");
    return ss.str();
#else
    // No put_time until gcc 5.
    char buf[128];
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", std::localtime(&t));
    return std::string(buf);
#endif
}

void ConsoleReporter::report() noexcept {
    printWithBanner(formatNow(), '=');
    printf("\n");

    auto counters = registry_->counters();
    if (!counters.empty()) {
        printWithBanner("-- Counters", '-');
        for (auto& entry : counters) {
            printf("%s\n", entry.first.c_str());
            printCounter(entry.second);
        }
        printf("\n");
    }

    auto timers = registry_->timers();
    if (!timers.empty()) {
        printWithBanner("-- Timers", '-');
        for (auto& entry : timers) {
            printf("%s\n", entry.first.c_str());
            printTimer(entry.second);
        }
        printf("\n");
    }
}

void ConsoleReporter::printCounter(Counter *counter) {
    printFormatted("count", "=", counter->value(), "");
}

void ConsoleReporter::printTimer(Timer *timer) {
    auto snap = timer->snapshot();
    printFormatted("count", "=", timer->count(), "");
    printFormatted("1-minute rate", "=", timer->oneMinuteRate(), "calls/s");
    printFormatted("5-minute rate", "=", timer->fiveMinuteRate(), "calls/s");
    printFormatted("15-minute rate", "=", timer->fifteenMinuteRate(), "calls/s");

    printFormatted("min", "=", snap.min(), "us");
    printFormatted("max", "=", snap.max(), "us");
    printFormatted("mean", "=", snap.mean(), "us");
    printFormatted("stdev", "=", snap.stdev(), "us");
    printFormatted("median", "=", snap.median(), "us");
    printFormatted("75%", "<=", snap.get75tile(), "us");
    printFormatted("95%", "<=", snap.get95tile(), "us");
    printFormatted("99%", "<=", snap.get99tile(), "us");
    printFormatted("99.9%", "<=", snap.get999tile(), "us");
}

void ConsoleReporter::printWithBanner(std::string const& str, char sym) {
    const int kConsoleWidth = 80; // Narrow console bigot :)

    assert(str.size() < kConsoleWidth);

    printf("%s %s\n", str.c_str(),
        std::string(kConsoleWidth - str.size(), sym).c_str());
}

PeriodicReporter* mkConsoleReporter(MetricRegistry *registry) {
    return new ConsoleReporter(registry);
}

} // ccmetrics namespace
