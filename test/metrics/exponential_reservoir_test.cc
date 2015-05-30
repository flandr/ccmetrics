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

#include "metrics/exponential_reservoir.h"

namespace ccmetrics {
namespace test {

TEST(ExponentialReservoirTest, BasicFunctionality) {
    ExponentialReservoir res;

    // Reservoir is big enough to hold 100 values; snapshots will be precise
    for (int i = 0; i <= 100; ++i) {
        res.update(i);
    }

    Snapshot snap = res.snapshot();
    ASSERT_EQ(0, snap.min());
    ASSERT_EQ(100, snap.max());
    ASSERT_EQ(50, snap.median());
    ASSERT_LT(99, snap.get99tile());
}

TEST(ExponentialReservoirTest, Sampling) {
    ExponentialReservoir res;

    res.update(1000);
    // Push out initial value
    for (int i = 0; i < 1E5; ++i) {
        res.update(1);
    }
    ASSERT_EQ(1, res.snapshot().max());
}

} // test namespace
} // ccmetrics namespace
