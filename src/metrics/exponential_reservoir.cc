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

#include "metrics/exponential_reservoir.h"

#include <cmath>

#include "thread_local_random.h"

namespace ccmetrics {

ExponentialReservoir::SMR
ExponentialReservoir::smr{{},
    {ExponentialReservoir::NewHPFunctor(), ExponentialReservoir::retireHazard}};

ExponentialReservoir::ExponentialReservoir()
        : data_(new Data(std::chrono::steady_clock::now())),
          next_scale_(CWG1778Hack(std::chrono::steady_clock::now() +
            std::chrono::hours(1))) { }

const double ExponentialReservoir::kAlpha = 0.015;
const double ExponentialReservoir::kSize = 1028;

ExponentialReservoir::~ExponentialReservoir() {
    smr.hp->retireNode(data_.load());
}

Snapshot ExponentialReservoir::snapshot() {
    std::lock_guard<std::mutex> lock(rescale_snap_mutex_);

    // Note that this is the one access that doesn't require a hazard pointer
    // because it is mutually exclusive with rescale, the only method that
    // can release the underlying map.
    auto& values = (*data_).map;
    return Snapshot(values.values(), /*sorted=*/ false);
}

// XXX needs mock clock for testing :/
template<typename TimePoint>
ExponentialReservoir::Data* ExponentialReservoir::loadAndRescaleIfNeeded(
        HazardPointers<Data>::pointer_type& hp, TimePoint now) {
    auto next = next_scale_.load(std::memory_order_acquire);
    if (now > next.t) {
        return rescale(hp, now, next);
    }
    return hp.loadAndSetHazard(data_, 0);
}


// Implements Priority Sampling [1], ranking entries by w_i/u_i, where u_i
// is drawn uniformly at random from (0, 1] and keeping only the top K entries.
//
// [1] N. Alon, et al. "Estimating sums of arbitrary selections with few
// probes." In PODS, 2005.
void ExponentialReservoir::update(int64_t value) {
	auto now = std::chrono::steady_clock::now();

	auto& hp = *smr.hp;
	auto* data = loadAndRescaleIfNeeded<decltype(now)>(hp, now); // hp held
	auto& values = data->map;

	double delta = std::chrono::duration<double>(now - data->landmark).count();
	double priority = std::exp(kAlpha * delta) /
		(1.0 - ThreadLocalRandom::current().nextDouble());

	if (data->count.fetch_add(1) < kSize) {
		values.insert(priority, value);
	}
	else {
		auto first = values.firstKey();
		if (first < priority && values.insert(priority, value)) {
			// Remove an item (may be racy w/ concurrent removal of first)
			// TODO: a specialized eraseFirst would be more efficient in CSLM
			while (!values.erase(first)) {
				first = values.firstKey();
			}
		}
	}

	hp.clearHazard(0);
}

template<typename TimePoint>
ExponentialReservoir::Data* ExponentialReservoir::rescale(
        HazardPointers<Data>::pointer_type& hp, TimePoint now,
        CWG1778Hack next) {
    if (!next_scale_.compare_exchange_strong(next,
            CWG1778Hack(next.t + std::chrono::hours(1)))) {
        return hp.loadAndSetHazard(data_, 0);
    }

    // We've won the race to be the rescaler; rejoice & prevent concurrent
    // snapshots.
    std::lock_guard<std::mutex> lock(rescale_snap_mutex_);

    // Construct a replacment map structure with the new landmark
    Data *nextdata = new Data(now);
    hp.setHazard(0, nextdata);

    Data *data = data_.load(std::memory_order_relaxed);
    data_.store(nextdata, std::memory_order_release);

    // Take a weak snapshot of the values in the original map. Note that
    // concurrent updates that observe the old values_ could still be inserting,
    // which could cause us to miss a few updates. Since this is a sampling
    // reservior this does not really matter, but do note that it causes
    // a potential burst of correlated skipped entries at each rescaling period.
    // With the default rescaling period (hourly) and default alpha (wighted
    // to last 5 minutes), this is most likely a meaningless concern.
    std::vector<std::pair<double, int64_t>> snap = data->map.entries();
    auto delta = std::chrono::duration<double>(now - data->landmark).count();

    // Drop it
    hp.retireNode(data);

    for (size_t i = 0; i < snap.size(); ++i) {
        // Rescale priorities in the snapshot
        snap[i].first *= std::exp(-kAlpha * delta);
    }

    // Expensive reinsertion. For most update patterns it is probably better
    // to swap the old map back in for the new and then insert the (expected
    // fewer) entries from the new map into the old, but rescaling is rare
    // so just doing the most straightforward thing for now.
    auto& values = nextdata->map;
    for (int i = snap.size() - 1; i >= 0; --i) {
        if (data->count.fetch_add(1) < kSize) {
            values.insert(snap[i].first, snap[i].second);
        } else {
            auto first = values.firstKey();
            if (first < snap[i].first &&
                    values.insert(snap[i].first, snap[i].second)) {
                while (!values.erase(first)) {
                    first = values.firstKey();
                }
            }
        }
    }
    return nextdata;
}

} // ccmetrics namespace
