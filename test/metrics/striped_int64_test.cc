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

#include <array>
#include <thread>

#include "metrics/striped_int64.h"

namespace ccmetrics {
namespace test {

// Really tests nothing w/o Valgrind
TEST(Striped64_StorageTest, BasicFuncationality) {
    Striped64_Storage s1;
    ASSERT_EQ(2U, s1.size());

    Striped64_Storage* s2 = Striped64_Storage::expand(&s1);
    EXPECT_EQ(4U, s2->size());
    s2->disavow();
    delete s2;

    Striped64_Storage* s3 = Striped64_Storage::expand(&s1);
    EXPECT_EQ(4U, s3->size());

    // Failure to disavow s1 will lead to double free
    s1.disavow_all();

    delete s3;
}

TEST(Striped64Test, BasicFunctionality) {
    Striped64 val;
    ASSERT_EQ(0, val.value());

    val.add(1);
    ASSERT_EQ(1, val.value());

    val.add(-1);
    ASSERT_EQ(0, val.value());

    Striped64 val2(4);
    ASSERT_EQ(0, val2.value());

    val2.add(1);
    ASSERT_EQ(1, val2.value());
}

// Non-deterministic but expected to exercise concurrent updates
TEST(Striped64Test, ConcurrencySmokeTest) {
    Striped64 val;
    const int K = 100000;   // 100000 updates per
    const int N = 4;        // 4-way workers

    auto work = [&]() -> void {
            for (int i = 0; i < K; ++i) { val.add(1); }
        };

    std::array<std::thread, N> workers { std::thread(work), std::thread(work),
        std::thread(work), std::thread(work) };

    for (auto i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }

    ASSERT_EQ(K * N, val.value());
}

} // test namespace
} // ccmetrics namespace
