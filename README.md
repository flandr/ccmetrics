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

## Supported platforms

ccmetrics is known to work on the following platforms / compilers:

 - Windows with Visual Studio 2013+ (dynamic linking only)
 - Linux with GCC 4.8
 - OS X 10.6+

It _should_ build with any compiler that supports C++11 language features like
`auto` type specification and `std::atomic` support. Platforms without a
[pthreads](http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html)
implementation will likely need to port the TLS implementation in
[thread_local_detail.h](src/detail/thread_local_detail.h).

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
