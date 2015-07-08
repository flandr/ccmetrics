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

#ifndef SRC_CCMETRICS_METRIC_REGISTRY_H_
#define SRC_CCMETRICS_METRIC_REGISTRY_H_

#include <map>
#include <string>

#include "ccmetrics/counter.h"
#include "ccmetrics/porting.h"
#include "ccmetrics/timer.h"

namespace ccmetrics {

class MetricRegistryImpl;

/**
 * Container for name -> metric mappings.
 *
 * N.B. that metric accessors currently incur global synchronization overhead,
 * and that statically-scoped reference handles are the most performant way
 * to access metrics. See [README.md](../../README.md) and the examples
 * for details.
 *
 * Metrics created through this registry are _forever_; there are no interfaces
 * to remove metrics other than by deleting the registry. It is up to the caller
 * to ensure that metric references are not used after registry deletion.
 */

class CCMETRICS_SYM MetricRegistry {
public:
    MetricRegistry();
    ~MetricRegistry();

    /** @return a new or existing counter. */
    Counter* counter(std::string const& name);

    /** @return a new or existing timer. */
    Timer* timer(std::string const& name);

    /** @return all registered counter metrics. */
    std::map<std::string, Counter*> counters() const;

    std::map<std::string, Timer*> timers() const;
private:
    MetricRegistry(MetricRegistry const&) = delete;
    MetricRegistry& operator=(MetricRegistry const&) = delete;
    MetricRegistryImpl *impl_;
};

/**
 * Increment the named counter from the registry.
 *
 * The counter reference binding is on-first-use; neither `name` nor `reference`
 * changes after that point will have any effect.
 *
 * @param name the counter name
 * @param registry a registry reference
 */
#define INCREMENT_COUNTER(name, registry)                       \
    do {                                                        \
    STATIC_DEFINE_ONCE(ccmetrics::Counter*, ANON_VAR(counter),  \
        registry.counter(name));                                \
    ANON_VAR(counter)->inc();                                   \
    }  while (0)

/**
 * Record the duration of execution whtin a scope.
 */
#define SCOPED_TIMER(name, registry)                            \
    STATIC_DEFINE_ONCE(ccmetrics::Timer*, ANON_VAR(timer),      \
        registry.timer(name));                                  \
    ccmetrics::ScopedTimer ANON_VAR(scoped_timer)(ANON_VAR(timer))

} // ccmetrics namespace

#endif // SRC_CCMETRICS_METRIC_REGISTRY_H_
