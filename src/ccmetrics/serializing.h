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

#ifndef SRC_CCMETRICS_SERIALIZING_H_
#define SRC_CCMETRICS_SERIALIZING_H_

#include <string>

#include "ccmetrics/counter.h"
#include "ccmetrics/metric_registry.h"
#include "ccmetrics/timer.h"

namespace ccmetrics {

template<typename Format>
class Serializer {
public:
    std::string serialize(Timer *timer) {
        return static_cast<Format*>(this)->do_serialize(timer);
    }

    std::string serialize(Counter *counter) {
        return static_cast<Format*>(this)->do_serialize(counter);
    }

    std::string serialize(MetricRegistry *registry) {
        return static_cast<Format*>(this)->do_serialize(registry);
    }
};

} // ccmetrics namespace

#endif // SRC_CCMETRICS_SERIALIZING_H_
