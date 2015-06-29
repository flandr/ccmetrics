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

#include "ccmetrics/metric_registry.h"
#include "ccmetrics/serializing/json_serializer.h"

namespace ccmetrics {
namespace test {

// Basically just exercises interfaces for Valgrind
TEST(SerializingTest, JsonSmoke) {
    MetricRegistry reg;

    Serializer<JsonSerializer> ser;
    ASSERT_EQ("{\"counters\":{},\"timers\":{}}", ser.serialize(&reg));

    Counter *c1 = reg.counter("foo");
    Counter *c2 = reg.counter("bar");
    ASSERT_EQ("{\"counters\":{\"bar\":{\"count\":0},\"foo\":{\"count\":0}},\"timers\":{}}",
        ser.serialize(&reg));
    std::string no_timers = ser.serialize(&reg);

    Timer *t1 = reg.timer("foo");
    ASSERT_NE(no_timers, ser.serialize(&reg));
}

} // test namespace
} // ccmetrics namespace
