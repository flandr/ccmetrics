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

#include <atomic>

#include "hazard_pointers.h"

namespace ccmetrics {
namespace test {

TEST(HazardPointerTest, TestAllocateAndRetire) {
    struct Tag { };
    HazardPointers<Tag, 1> pointers;
    auto* hp1 = pointers.allocate();
    ASSERT_FALSE(hp1 == nullptr);
    auto* hp2 = pointers.allocate();
    ASSERT_NE(hp1, hp2);

    pointers.retire(hp1);
    auto* hp3 = pointers.allocate();
    // Reused
    ASSERT_EQ(hp1, hp3);
}

class DeleteCounter {
public:
    explicit DeleteCounter(std::atomic<int> *dc) : delete_count_(dc) { }
    ~DeleteCounter() { ++(*delete_count_); }
private:
    std::atomic<int> *delete_count_;
};

TEST(HazardPointerTest, RetireNodeTriggersScan) {
    HazardPointers<DeleteCounter> pointers;
    auto* hp1 = pointers.allocate();

    // There's only one HP in the system, so we should expect to see
    // clean up every 1.25 times that a node is retired
    std::atomic<int> deletions(0);
    auto ptr1 = new DeleteCounter(&deletions);
    auto ptr2 = new DeleteCounter(&deletions);

    hp1->retireNode(ptr1);
    ASSERT_EQ(0, deletions.load());
    hp1->retireNode(ptr2);
    // Neither were actually held by a hazard pointer, so both were deleted
    ASSERT_EQ(2, deletions.load());
}

TEST(HazardPointerTest, ScanPreservesLiveItems) {
    HazardPointers<DeleteCounter> pointers;
    auto* hp1 = pointers.allocate();

    // There's only one HP in the system, so we should expect to see
    // clean up every 1.25 times that a node is retired
    std::atomic<int> deletions(0);
    auto ptr1 = new DeleteCounter(&deletions);
    auto ptr2 = new DeleteCounter(&deletions);
    auto ptr3 = new DeleteCounter(&deletions);

    // Set the hazard pointer for ptr1
    hp1->setHazard(0, ptr1);

    hp1->retireNode(ptr1);
    ASSERT_EQ(0, deletions.load());
    hp1->retireNode(ptr2);
    ASSERT_EQ(1, deletions.load());

    // Release the hazard pointer for ptr1
    hp1->clearHazard(0);
    hp1->retireNode(ptr3);

    // Now everything was deleted
    ASSERT_EQ(3, deletions.load());
}

TEST(HazardPointerTest, HelpScanCleansUpAfterLazyBones) {
    class TestableHP : public HazardPointer<DeleteCounter> {
    public: using HazardPointer<DeleteCounter, 1>::scan;
    };

    HazardPointers<DeleteCounter> pointers;
    auto* hp1 = static_cast<TestableHP*>(pointers.allocate());
    auto* lazy = pointers.allocate();

    std::atomic<int> deletions(0);

    auto ptr1 = new DeleteCounter(&deletions);
    auto ptr2 = new DeleteCounter(&deletions);
    auto ptr3 = new DeleteCounter(&deletions);
    auto ptr4 = new DeleteCounter(&deletions);

    lazy->retireNode(ptr1);
    // Go away w/o running reclamation
    pointers.retire(lazy);

    ASSERT_EQ(0, deletions.load());
    hp1->retireNode(ptr2);
    hp1->retireNode(ptr3);
    // Note that we need one more pointer to exceed the limit now that there
    // are two hazard pointers in the system
    hp1->retireNode(ptr4);

    // Force the scan; picking up just the one pointer off of somebody else's
    // list will not trigger another scan of our retire list (that would happen
    // after subsequent uses & retires on hp1)
    hp1->scan();

    // Everything was cleaned up
    ASSERT_EQ(4, deletions.load());
}

} // test namespace
} // ccmetrics namespace
