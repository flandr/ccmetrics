# Metrics for C++ programs

Fine-grained application metrics and reporting.

 - Event counters
 - Timers w/ distribution estimates & percentiles
 - One, five, fifteen minute rates

Usage, TL;DR edition:

```
ccmetrics::MetricRegistry registry;

void work() {
    UPDATE_COUNTER("work", registry);

    if (fast_path) {

        //
        // stuff
        //

    } else {
        SCOPED_TIMER("slow", registry);

        //
        // expensive stuff
        //
    }
}

//
// Reporting
//

for (auto& entry : registry.counters()) {
    printf("%s %" PRId64"\n", entry.first.c_str(),
        entry.second->value());
}

for (auto& entry : registry.timers()) {
    printf("%s %" PRId64" %f %f %f\n", entry.first.c_str(),
        entry.second->count(),
        entry.second->oneMinuteRate(),
        entry.second->fiveMinuteRate(),
        entry.second->fifteenMinuteRate());
    if (g_verbose) {
        printf("99th percentile: %f\n", entry.second->snapshot().get99tile());
    }
}

```

_Tip of the hat to [Coda Hale /
Yammer](https://github.com/dropwizard/metrics)._

## License

Copyright © 2015 Nathan Rosenblum <flander@gmail.com>

Licensed under the MIT License.

## References

This library was inspired by the excellent
Java [metrics](https://github.com/dropwizard/metrics) library, and owes many
of its interfaces to that project.

ccmetrics uses several other open-source libraries:

 - [RapidJSON](https://github.com/miloyip/rapidjson) for JSON serialization
 - [Google Test](https://code.google.com/p/googletest) for unit tests
