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

#ifndef SRC_THREAD_LOCAL_RANDOM_H_
#define SRC_THREAD_LOCAL_RANDOM_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <random>
#include <type_traits>

#include "thread_local.h"

namespace ccmetrics {

/**
 * A thread-local source of randomness.
 *
 * This class uses a linear congruential generator internally and does
 * not produce cryptographically secure random numbers.
 */
class ThreadLocalRandom {
public:
    /** @return the current thread's thread-local random. */
    static ThreadLocalRandom& current();

    /** Fills the input range with random values. */
    template<typename Iter>
    void generate(Iter begin, Iter end);

    /** @return the next random value in [0, numeric_limits<int64_t>::max]. */
    int64_t next();

protected:
    // Visible for testing
    ThreadLocalRandom();

private:
    // Evolving seed state for generating unique per-thread streams
    static std::atomic<uint32_t> seeder_;

    static ThreadLocal<ThreadLocalRandom> local_random_;

    struct State {
        std::minstd_rand generator;
        explicit State(uint32_t seed) : generator(seed) { }
    };

    State state_;

    // Ugh.
    template<typename T>
    friend T* ThreadLocal<T>::get() const;
};

template<typename Iter>
void ThreadLocalRandom::generate(Iter begin, Iter end) {
    const uint8_t width = sizeof(*begin);
    static_assert(width <= 8, "Only implemented for <= 64 bit values");

    std::uniform_int_distribution<uint64_t> random;
    for ( ; begin != end; ++begin) {
        // TODO: This could be a lot faster by processing at `width` increments
        uint8_t *ptr = reinterpret_cast<uint8_t*>(&(*begin));
        uint64_t r = random(state_.generator);
        for (uint8_t i = 0; i < width; ++i) {
            ptr[i] = r & 0xFF;
            r >>= 8;
        }
    }
}

} // ccmetrics namespace

#endif // SRC_THREAD_LOCAL_RANDOM_H_
