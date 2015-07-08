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

#include "ccmetrics/serializing/json_serializer.h"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#if defined(_WIN32)
#define constexpr
#endif

namespace ccmetrics {

namespace {
template<typename Writer, typename T>
void writeNumeric(Writer &writer, std::string const& key, T value) {
    writer.String(key.c_str());
    writer.Int64(value);
}

template<typename Writer>
void writeNumeric(Writer &writer, std::string const& key, double value) {
    writer.String(key.c_str());
    writer.Double(value);
}

template<typename Writer>
void serialize_helper(Timer *timer, Writer &writer) {
    // us -> s
    constexpr const double kFactor = 1.0 / 1E6;

    writer.StartObject();

    Snapshot snap = timer->snapshot();

    writeNumeric(writer, "count", timer->count());
    writeNumeric(writer, "max", snap.max() * kFactor);
    writeNumeric(writer, "mean", snap.mean() * kFactor);
    writeNumeric(writer, "min", snap.min() * kFactor);

    writeNumeric(writer, "p50", snap.median() * kFactor);
    writeNumeric(writer, "p75", snap.get75tile() * kFactor);
    writeNumeric(writer, "p95", snap.get95tile() * kFactor);
    writeNumeric(writer, "p99", snap.get99tile() * kFactor);
    writeNumeric(writer, "p999", snap.get999tile() * kFactor);

    writeNumeric(writer, "stdev", snap.stdev() * kFactor);
    writeNumeric(writer, "m15_rate", timer->fifteenMinuteRate());
    writeNumeric(writer, "m5_rate", timer->fiveMinuteRate());
    writeNumeric(writer, "m1_rate", timer->oneMinuteRate());

    writer.EndObject();
}

template<typename Writer>
void serialize_helper(Counter *counter, Writer &writer) {
    writer.StartObject();

    writeNumeric(writer, "count", counter->value());

    writer.EndObject();
}

} // unnamed namespace

std::string JsonSerializer::do_serialize(Timer *timer) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    serialize_helper(timer, writer);

    return buffer.GetString();
}

std::string JsonSerializer::do_serialize(Counter *counter) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    serialize_helper(counter, writer);

    return buffer.GetString();
}

std::string JsonSerializer::do_serialize(MetricRegistry *registry) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.StartObject();

    writer.String("counters");
    writer.StartObject();
    for (auto const& entry : registry->counters()) {
        writer.String(entry.first.c_str());
        serialize_helper(entry.second, writer);
    }
    writer.EndObject();

    writer.String("timers");
    writer.StartObject();
    for (auto const& entry : registry->timers()) {
        writer.String(entry.first.c_str());
        serialize_helper(entry.second, writer);
    }
    writer.EndObject();

    writer.EndObject();
    return buffer.GetString();
}

} // ccmetrics namespace
