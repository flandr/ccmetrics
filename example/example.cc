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

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include <stdlib.h>
#include <inttypes.h>

#include <thread>

#include "ccmetrics/counter.h"
#include "ccmetrics/detail/define_once.h"
#include "ccmetrics/metric_registry.h"
#include "ccmetrics/reporting/periodic_reporter.h"

ccmetrics::MetricRegistry& registry() {
    // XXX If you're on MSVC < VS 2013 you need better statics. Better
    // statics coming, I promise.
    static ccmetrics::MetricRegistry singleton;
    return singleton;
}

void foo(int iters) {
    for (int i = 0; i < iters; ++i) {
        INCREMENT_COUNTER("foo", registry());
    }
}

void bar(int iters) {
    for (int i = 0; i < iters; ++i) {
        INCREMENT_COUNTER("bar", registry());
    }
}

void slow(int iters) {
    SCOPED_TIMER("slow", registry());
    for (int i = 0; i < iters; ++i) {
        SCOPED_TIMER("fast", registry());
        UPDATE_METER("metered.event", registry(), 1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

int main(int argc, char **argv) {
#if defined(_WIN32)
    // Initialize WSA
    WORD version = MAKEWORD(2, 2);
    WSADATA data;
    WSAStartup(version, &data);
#endif

    int iters = 1000;
    if (argc == 2) {
        iters = atoi(argv[1]);
    }
    foo(iters);
    bar(iters);
    slow(iters);

    auto reporter = ccmetrics::mkConsoleReporter(&registry());
    reporter->report();
    return 0;
}
