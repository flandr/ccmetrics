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

#include "ccmetrics/snapshot.h"

#include <algorithm>
#include <cmath>
#include <exception>

namespace ccmetrics {

Snapshot::Snapshot(std::vector<int64_t> &&values, bool sorted)
        : values_(new std::vector<int64_t>(std::move(values))) {
    if (!sorted) {
        std::sort(values_->begin(), values_->end());
    }
}

Snapshot::~Snapshot() {
    delete values_;
}

double Snapshot::mean() const {
    if (values_->empty()) {
        return 0;
    }

    int64_t sum = 0;
    for (int64_t v : *values_) {
        sum += v;
    }
    return sum / static_cast<double>(values_->size());
}

double Snapshot::stdev() const {
    // Wellford's algorithm (numerically stable online variance)
    int n = 0;
    int64_t varsum = 0;
    double mean = 0.0;
    for (int64_t value : *values_) {
        ++n;
        double delta = value - mean;
        mean += delta / n;
        varsum += delta * (value - mean);
    }

    if (n < 2) {
        return 0;
    }

    double variance = varsum / static_cast<double>(n - 1);
    return ::sqrt(variance);
}

double Snapshot::valueAt(double quantile) const {
    if (quantile < 0.0 || quantile > 1.0) {
        throw std::invalid_argument("quantile must be in [0, 1]");
    }

    if (values_->empty()) {
        return 0;
    }

    // Uses R-7 (default in R, and also in S), linear interpoloation of the
    // modes for the order statistics of U[0, 1].
    double idx = quantile * (values_->size() + 1);

    if (idx < 1) {
        return (*values_)[0];
    } else if (idx >= values_->size()) {
        return values_->back();
    }

    double x_h = (*values_)[static_cast<int>(idx - 1)];
    double x_hnext = (*values_)[static_cast<int>(idx)];
    return x_h + (idx - ::floor(idx)) * (x_hnext - x_h);
}

} // ccmetrics namespace
