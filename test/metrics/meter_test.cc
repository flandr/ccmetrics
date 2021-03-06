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

#include "metrics/meter_impl.h"

namespace ccmetrics {
namespace test {

class RateEWMATest : public ::testing::Test {
public:
    void tick(RateEWMA &rate) {
        rate.tick();
    }
};

// Brittle under debuggers or valgrind, fyi. Could use a mock clock.
TEST_F(RateEWMATest, BasicFunctionality) {
    RateEWMA rate(MeterImpl::kOneMinuteAlpha);
    rate.update(1);
    tick(rate); // Force tick to 5s
    ASSERT_EQ(1.0 / static_cast<double>(RateEWMA::kInterval), rate.rate());

    rate.update(1);
    tick(rate); // Force tick to 10s
    ASSERT_EQ(1.0 / static_cast<double>(RateEWMA::kInterval), rate.rate());

    rate.update(0);
    tick(rate); // Force tick to 15s
    ASSERT_LT(rate.rate(), 1.0 / static_cast<double>(RateEWMA::kInterval));
}

} // test namespace
} // ccmetrics namespace
