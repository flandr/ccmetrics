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

#include <initializer_list>

#include "ccmetrics/snapshot.h"

namespace ccmetrics {
namespace test {

static Snapshot mkSnap(std::initializer_list<int64_t> l) {
    return Snapshot(l, /*sorted=*/ false);
}

TEST(SnapshotTest, Mean) {
    ASSERT_EQ(0.0, mkSnap({}).mean());
    ASSERT_EQ(0.5, mkSnap({0, 1}).mean());
    ASSERT_EQ(0, mkSnap({-1, 1}).mean());
}

TEST(SnapshotTest, Stdev) {
    ASSERT_EQ(0.0, mkSnap({}).stdev());
    ASSERT_EQ(0.0, mkSnap({1}).stdev());
    ASSERT_EQ(0.0, mkSnap({2, 2}).stdev());
    ASSERT_EQ(1.0, mkSnap({1, 3, 3}).stdev());
}

TEST(SnapshotTest, Min) {
    ASSERT_EQ(0, mkSnap({}).min());
    ASSERT_EQ(-1, mkSnap({-1, 2, 3}).min());
}

TEST(SnapshotTest, Max) {
    ASSERT_EQ(0, mkSnap({}).max());
    ASSERT_EQ(3, mkSnap({3, 2, -1}).max());
}

TEST(SnapshotTest, Median) {
    ASSERT_EQ(0.0, mkSnap({}).median());
    ASSERT_EQ(2.0, mkSnap({1, 2, 3}).median());
    ASSERT_EQ(2.5, mkSnap({1, 2, 3, 4}).median());
}

TEST(SnapshotTest, UnsortedInputsGetSorted) {
    Snapshot snap({3, 1, 2}, false);
    ASSERT_EQ(2.0, snap.median());
}

} // test namespace
} // ccmetrics namespace
