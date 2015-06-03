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

#ifndef SRC_CCMETRICS_SNAPSHOT_H_
#define SRC_CCMETRICS_SNAPSHOT_H_

#include <cinttypes>
#include <vector>

namespace ccmetrics {

/** A snapshot of a distribution. */
class Snapshot {
public:
    Snapshot(std::vector<int64_t> &&values, bool sorted);

    /** @return the mean. */
    double mean() const;

    /** @return the standard deviation. */
    double stdev() const;

    /** @return the minimum value. */
    int64_t min() const {
        if (!values_.empty()) {
            return values_.front();
        }
        return 0;
    }

    /** @return the maximum value. */
    int64_t max() const {
        if (!values_.empty()) {
            return values_.back();
        }
        return 0;
    }

    /** @return the median. */
    double median() const { return valueAt(0.5); }

    /** @return the 75th percentile. */
    double get75tile() const { return valueAt(0.75); }

    /** @return the 95th precentile. */
    double get95tile() const { return valueAt(0.95); }

    /** @return the 99th precentile. */
    double get99tile() const { return valueAt(0.99); }

    /** @return the 99.9th percentil. */
    double get999tile() const { return valueAt(0.999); }

    /** @return the valuue of the distribution at the quantile [0, 1] */
    double valueAt(double quantile) const;
private:
    std::vector<int64_t> values_;
};

} // ccmetrics namespace

#endif // SRC_CCMETRICS_SNAPSHOT_H_
