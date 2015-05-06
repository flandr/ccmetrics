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

#include "thread_local.h"

namespace ccmetrics {
namespace test {

class TestClass {
public:
    explicit TestClass(bool *deleted) : deleted_(deleted), val_(0) { }
    ~TestClass() {
        *deleted_ = true;
    }

    void set(int val) { val_ = val; }
    int get() const { return val_; }
private:
    bool* deleted_;
    int val_;
};

template<typename T>
class TestableTLP : public ThreadLocalPointer<T> {
public:
    using ThreadLocalPointer<T>::id; // Making public
};

TEST(ThreadLocalPointerTest, BasicFunctionality) {
    bool deleted = false;

    {
        ThreadLocalPointer<TestClass> ptr;
        ptr.reset(new TestClass(&deleted));

        ptr->set(31337);
        ASSERT_EQ(31337, ptr->get());

        (*ptr).set(42);
        ASSERT_EQ(42, (*ptr).get());
    }

    ASSERT_TRUE(deleted);
}

TEST(ThreadLocalPointerTest, IdsReused) {
    bool deleted1 = false, deleted2 = false, deleted3 = false;
    uint32_t id1, id2, id3;

    {
        TestableTLP<TestClass> ptr;
        ptr.reset(new TestClass(&deleted1));
        id1 = ptr.id();
    }

    {
        TestableTLP<TestClass> ptr;
        ptr.reset(new TestClass(&deleted2));
        id2 = ptr.id();

        TestableTLP<TestClass> ptr3;
        ptr3.reset(new TestClass(&deleted3));
        id3 = ptr3.id();
    }

    ASSERT_TRUE(deleted1);
    ASSERT_TRUE(deleted2);
    ASSERT_TRUE(deleted3);
    ASSERT_EQ(id1, id2);
    ASSERT_NE(id2, id3);
}

// On could argue that this is really the "basic functionality" test...
TEST(ThreadLocalPointerTest, VerifyThreadLocality) {
    ThreadLocalPointer<int> pointer;
    pointer.reset(new int(2));
    auto work = [&pointer]() -> void {
            pointer.reset(new int(3));
        };
    std::thread(work).join();
    ASSERT_EQ(2, *pointer); // Our local value was not affected
}

TEST(ThreadLocalTest, BasicFunctionality) {
    ThreadLocal<int> tl;
    ASSERT_EQ(0, *tl);

    *tl = 1;
    ASSERT_EQ(1, *tl);
    ++(*tl);
    ASSERT_EQ(2, *tl);

    auto work = [&tl]() -> void {
            ++(*tl);
            // As a thread-local, it starts of at default (0)
            ASSERT_EQ(1, *tl);
        };
    std::thread(work).join();
    ASSERT_EQ(2, *tl);
}

static int custom_deletions = 0;

template<typename T>
static void custom_deleter(void *d) {
    ++custom_deletions;
    delete reinterpret_cast<T*>(d);
}

TEST(ThreadLocalTest, CustomDeleter) {
    {
        ThreadLocal<int> t1(DefaultNew<int>(), custom_deleter<int>);
        *t1 = 1;
    }
    ASSERT_EQ(1, custom_deletions);
}

} // test namespace
} // ccmetrics namespace
