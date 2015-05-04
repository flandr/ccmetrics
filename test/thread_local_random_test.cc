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

#include <thread>

#include <gtest/gtest.h>

#include "thread_local_random.h"

#include <inttypes.h>

namespace ccmetrics {
namespace test {

class TestableThreadLocalRandom : public ThreadLocalRandom {
public:
    using ThreadLocalRandom::ThreadLocalRandom;
};

TEST(ThreadLocalRandomTest, BasicFunctionality) {
    TestableThreadLocalRandom random;
    auto v1 = random.next();
    auto v2 = random.next();
    // Flakey but low likelihood of false positive. Unit testing randomness
    // is a terrible idea.
    ASSERT_NE(v1, v2);

    std::vector<int64_t> randints(10);
    std::vector<char> randchars(10);

    random.generate(randints.begin(), randints.end());
    random.generate(randchars.begin(), randchars.end());
    // Honestly you have basically no idea of how to "unit test" this.
}

TEST(ThreadLocalRandomTest, VerifyThreadLocality) {
    int64_t v1 = 0, v2 = 0;
    auto& random = ThreadLocalRandom::current();

    // This thread
    v1 = random.next();

    // Another thread
    std::thread([&v2]() -> void { v2 = ThreadLocalRandom::current().next(); })
        .join();

    // This doesn't really assert that we're dealing with different
    // streams :/ Perhaps add an explicit seed and compare output of
    // single-thread stream w/ local-thread stream (match) and other-
    // thread stream (no match).
    ASSERT_NE(v1, v2);
}

} // test namespace
} // ccmetrics namespace
