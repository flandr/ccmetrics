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

#include "thread_local_random.h"

#include <chrono>
#include <limits>

namespace ccmetrics {

ThreadLocalRandom& ThreadLocalRandom::current() {
    return *local_random_;
}

int64_t ThreadLocalRandom::next() {
    return std::uniform_int_distribution<int64_t>(0,
        std::numeric_limits<int64_t>::max())(state_.generator);
}

double ThreadLocalRandom::nextDouble() {
    return std::uniform_real_distribution<double>(0, 1)(state_.generator);
}

static uint32_t advanceSeed(std::atomic<uint32_t> *seed) {
    // Using (arbitrarily) H_0 from SHA-256 as the increment
    return (*seed) += 0x6a09e667U;
}

ThreadLocalRandom::ThreadLocalRandom() : state_(advanceSeed(&seeder_)) { }

static uint32_t mkInitialSeed() {
    // Terrible seed :/
    return std::chrono::system_clock::now().time_since_epoch().count();
}

std::atomic<uint32_t> ThreadLocalRandom::seeder_(mkInitialSeed());
ThreadLocal<ThreadLocalRandom, ThreadLocalRandom::NewFunctor> ThreadLocalRandom::local_random_;

} // ccmetrics namespace
