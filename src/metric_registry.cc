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

#include "ccmetrics/metric_registry.h"

#include <utility>

#include "metric_registry_impl.h"

namespace ccmetrics {

MetricRegistryImpl::MetricRegistryImpl() { }

namespace {
template<typename T>
void deleteMetrics(MetricMap<T> &mm) {
    std::lock_guard<std::mutex> lock(mm.mutex);
    for (auto& it : mm.metrics) {
        delete it.second;
    }
}

template<typename T>
std::map<std::string, T*> toMap(MetricMap<T> const& mm) {
    std::map<std::string, T*> ret;
    std::lock_guard<std::mutex> lock(mm.mutex);
    for (auto& entry : mm.metrics) {
        ret.insert(entry);
    }
    return ret;
}
} // unnamed namespace

MetricRegistryImpl::~MetricRegistryImpl() {
    deleteMetrics(counters_);
    deleteMetrics(timers_);
}

Counter* MetricRegistryImpl::counter(std::string const& name) {
    std::lock_guard<std::mutex> lock(counters_.mutex);
    auto exist = counters_.metrics.find(name);
    if (exist != counters_.metrics.end()) {
        return exist->second;
    }
    Counter *ret = new Counter();
    counters_.metrics.insert(std::make_pair(name, ret));
    return ret;
}

Timer* MetricRegistryImpl::timer(std::string const& name) {
    std::lock_guard<std::mutex> lock(timers_.mutex);
    auto exist = timers_.metrics.find(name);
    if (exist != timers_.metrics.end()) {
        return exist->second;
    }
    Timer *ret = new Timer();
    timers_.metrics.insert(std::make_pair(name, ret));
    return ret;
}

Meter* MetricRegistryImpl::meter(std::string const& name) {
    std::lock_guard<std::mutex> lock(meters_.mutex);
    auto exist = meters_.metrics.find(name);
    if (exist != meters_.metrics.end()) {
        return exist->second;
    }
    Meter *ret = new Meter();
    meters_.metrics.insert(std::make_pair(name, ret));
    return ret;
}

std::map<std::string, Counter*> MetricRegistryImpl::counters() const {
    return toMap(counters_);
}

std::map<std::string, Timer*> MetricRegistryImpl::timers() const {
    return toMap(timers_);
}

std::map<std::string, Meter*> MetricRegistryImpl::meters() const {
    return toMap(meters_);
}

//
// MetricRegistry
//

MetricRegistry::MetricRegistry() : impl_(new MetricRegistryImpl()) { }
MetricRegistry::~MetricRegistry() { delete impl_; }
Counter* MetricRegistry::counter(std::string const& name) {
    return impl_->counter(name);
}
std::map<std::string, Counter*> MetricRegistry::counters() const {
    return impl_->counters();
}
Timer* MetricRegistry::timer(std::string const& name) {
    return impl_->timer(name);
}
Meter* MetricRegistry::meter(std::string const& name) {
    return impl_->meter(name);
}
std::map<std::string, Timer*> MetricRegistry::timers() const {
    return impl_->timers();
}
std::map<std::string, Meter*> MetricRegistry::meters() const {
    return impl_->meters();
}

} // ccmetrics namespace
