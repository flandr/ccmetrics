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

#ifndef SRC_METRICS_EXPONENTIAL_RESERVOIR_H_
#define SRC_METRICS_EXPONENTIAL_RESERVOIR_H_

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <mutex>

#include "ccmetrics/snapshot.h"
#include "concurrent_skip_list_map.h"
#include "hazard_pointers.h"
#include "metrics/cwg1778hack.h"

namespace ccmetrics {

/**
 * Exponential decay sampling reservoir [1].
 *
 * This sampling method makes strong assumptions on a normal distribution of
 * values, which is probably not the right thing _almost all the time_ when
 * measuring request latencies of a system. As an alternative, consider using
 * the HdrReservoir (TODO: not yet implemented), which consumes much more but
 * provides fixed precision for percentil estimates.
 *
 * [1] http://dimacs.rutgers.edu/~graham/pubs/papers/fwddecay.pdf
 */
class ExponentialReservoir {
public:
    ExponentialReservoir();
    ~ExponentialReservoir();

    void update(int64_t value);
    Snapshot snapshot();
private:
    // Decay factor
    static constexpr double kAlpha = 0.015;
    // Number of elements in reservoir
    static constexpr double kSize = 1028;

    struct Data {
        // Map from priority -> value, for maintaining ordered decaying weights
        ConcurrentSkipListMap<double, int64_t> map;
        // Map size
        std::atomic<size_t> count;
        // Landmark for calculating weights
        decltype(std::chrono::steady_clock::now()) landmark;

        explicit Data(decltype(landmark) time) : count(0), landmark(time) { }
    };

    // XXX this is starting to feel like onerous boilerplate...
    struct NewHPFunctor {
        typename HazardPointers<Data>::pointer_type* operator()(void) const {
            return smr.hazards.allocate();
        }
    };
    static struct SMR {
        HazardPointers<Data> hazards;
        ThreadLocal<typename decltype(hazards)::pointer_type, NewHPFunctor> hp;
    } smr;
    static void retireHazard(void *h) {
        smr.hazards.retire(reinterpret_cast<
            typename decltype(smr.hazards)::pointer_type*>(h));
    }

    std::atomic<Data*> data_;

    // Time point for next rescaling
    std::atomic<CWG1778Hack> next_scale_;

    // Used to coordinate rescaling & snapshots, neither of which are on
    // the common/fast path
    std::mutex rescale_snap_mutex_;

    template<typename TimePoint>
    Data* loadAndRescaleIfNeeded(typename decltype(smr.hazards)::pointer_type&,
        TimePoint now);
    template<typename TimePoint>
    Data* rescale(typename decltype(smr.hazards)::pointer_type&, TimePoint now,
        CWG1778Hack next);
};

} // ccmetrics namespace

#endif // SRC_METRICS_EXPONENTIAL_RESERVOIR_H_
