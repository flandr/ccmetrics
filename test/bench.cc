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

#include <stdio.h>
#include <stdlib.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "metrics/striped_int64.h"

struct AtomicWrapper {
    std::atomic<int64_t> val;
    void add(int64_t delta) { val += delta; }
};

template<typename T>
std::chrono::milliseconds run(T &val, const int K, const int N) {
    auto start = std::chrono::system_clock::now();

    auto work = [&]() -> void {
            for (int i = 0; i < K; ++i) { val.add(1); }
        };

    std::vector<std::thread> workers;
    workers.reserve(N);
    for (int i = 0; i < N; ++i) {
        workers.emplace_back(std::thread(work));
    }

    for (auto i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }

    auto end = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s [threads] [iters]\n", argv[0]);
        exit(1);
    }

    int threads = atoi(argv[1]);
    int iters = atoi(argv[2]);

    AtomicWrapper aval;
    auto atomics = run(aval, iters, threads);

    ccmetrics::Striped64 sval;
    auto stripes = run(sval, iters, threads);

    printf("Atomics: %lld ms Stripes: %lld ms\n", atomics.count(),
           stripes.count());

    return 0;
}
