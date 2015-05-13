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

#include <exception>
#include <map>
#include <thread>

#include <gtest/gtest.h>

#include "concurrent_skip_list_map.h"
#include "thread_local_random.h"

namespace ccmetrics {
namespace test {

template<typename Map, typename Key>
typename Map::value_type getOrThrow(Map &map, Key const& key) {
    typename Map::value_type ret;
    if (!map.find(key, &ret)) {
        throw std::runtime_error("Fail");
    }
    return ret;
}

template<typename Key, typename Value>
class MapWrapper {
public:
    typedef Value value_type;

    bool find(Key const& key, Value *value) {
        auto it = map.find(key);
        if (it == map.end()) {
            return false;
        }
        *value = it->second;
        return true;
    }

    bool exists(Key const& key) {
        return map.find(key) != map.end();
    }

    bool erase(Key const& key) {
        return 1U == map.erase(key);
    }

    bool insert(Key const& key, Value const& value) {
        auto res = map.insert(std::make_pair(key, value));
        return res.second;
    }
private:
    std::map<Key, Value> map;
};

TEST(ConcurrentSkipListMapTest, StdMap) {
    MapWrapper<int, int> map;

    const int kSize = 10000;

    for (int i = 0; i < kSize; ++i) {
        //map.insert(i, i * 100);
        ASSERT_TRUE(map.insert(i, i * 100));
        ASSERT_TRUE(map.exists(i));
        ASSERT_EQ(i * 100, getOrThrow(map, i));
    }

    // Erase first works
    ASSERT_TRUE(map.erase(0));
    ASSERT_FALSE(map.exists(0));

    // Erase last works
    ASSERT_TRUE(map.erase(kSize - 1));
    ASSERT_FALSE(map.exists(kSize - 1));

    // Erase mid works
    ASSERT_TRUE(map.erase(kSize / 2));
    ASSERT_FALSE(map.exists(kSize / 2));

    // Everything else still exists
    for (int i = 0; i < kSize; ++i) {
        if (i == 0 || i == kSize - 1 || i == kSize / 2) {
            continue;
        }
        ASSERT_TRUE(map.exists(i));
    }

    // You can't insert over existing items
    ASSERT_FALSE(map.insert(1, 1));

    // Missing items aren't present
    ASSERT_FALSE(map.exists(kSize));

    // Likewise, you can't look them up
    int val;
    ASSERT_FALSE(map.find(kSize, &val));
}

TEST(ConcurrentSkipListMapTest, BasicFunctionality) {
    ConcurrentSkipListMap<int, int> map;

    const int kSize = 10000;

    for (int i = 0; i < kSize; ++i) {
        //map.insert(i, i * 100);
        ASSERT_TRUE(map.insert(i, i * 100));
        ASSERT_TRUE(map.exists(i));
        ASSERT_EQ(i * 100, getOrThrow(map, i));
    }

    // Erase first works
    ASSERT_TRUE(map.erase(0));
    ASSERT_FALSE(map.exists(0));

    // Erase last works
    ASSERT_TRUE(map.erase(kSize - 1));
    ASSERT_FALSE(map.exists(kSize - 1));

    // Erase mid works
    ASSERT_TRUE(map.erase(kSize / 2));
    ASSERT_FALSE(map.exists(kSize / 2));

    // Everything else still exists
    for (int i = 0; i < kSize; ++i) {
        if (i == 0 || i == kSize - 1 || i == kSize / 2) {
            continue;
        }
        ASSERT_TRUE(map.exists(i));
    }

    // You can't insert over existing items
    ASSERT_FALSE(map.insert(1, 1));

    // Missing items aren't present
    ASSERT_FALSE(map.exists(kSize));

    // Likewise, you can't look them up
    int val;
    ASSERT_FALSE(map.find(kSize, &val));
}

TEST(ConcurrentSkipListMapTest, ConcurrentMutationStress) {
    ConcurrentSkipListMap<int, int> map;
    const int kSize = 1000;

    auto work = [&](const int id) -> void {
            auto random = ThreadLocalRandom::current();
            for (int i = 0; i < 1E5; ++i) {
                int key = random.next() % kSize;
                map.insert(key, id);
                int op = key % 100;
                if (op < 80) {
                    // 80% read
                    int value = 0;
                    map.find(key, &value);
                } else if (op < 90) {
                    // 10% insert
                    map.insert(key, id);
                } else {
                    // 10% erase
                    map.erase(key);
                }
            }
        };

    auto t1 = std::thread(std::bind(work, 1));
    auto t2 = std::thread(std::bind(work, 2));

    t1.join();
    t2.join();
}

} // test namespace
} // ccmetrics namespace
