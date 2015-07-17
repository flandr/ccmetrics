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

#include <gtest/gtest.h>

#include <random>
#include <memory>
#include <thread>

#include "ccmetrics/metric_registry.h"
#include "ccmetrics/porting.h"
#include "ccmetrics/reporting/periodic_reporter.h"

namespace ccmetrics {
namespace test {

class TestReporter final : public PeriodicReporter {
public:
    void report() NOEXCEPT {
        ++invocations;
    }
    int invocations = 0;
};

TEST(ReportingTest, BasicFunctionality) {
    TestReporter reporter;
    reporter.report();
    ASSERT_EQ(1, reporter.invocations);

    reporter.start(std::chrono::milliseconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // Just assert >= 2; no matter what this is a racy test
    reporter.stop();
    ASSERT_GT(reporter.invocations, 1);
}

TEST(ReportingTest, ConsoleReporterSmokeTest) {
    MetricRegistry registry;
    auto reporter = mkConsoleReporter(&registry);
    reporter->report();

    auto counter = registry.counter("foo_counter");
    counter->update(7);

    std::minstd_rand gen;
    std::uniform_int_distribution<int> dist(0, 1000);
    auto timer = registry.timer("bar_timer");
    for (int i = 0; i < 1E5; ++i) {
        timer->update(dist(gen));
    }

    reporter->report();
}

} // test namespace
} // ccmetrics namespace
