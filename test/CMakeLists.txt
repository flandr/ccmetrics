project(tests C CXX)

# Set includes
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${gtest_INCLUDE_DIRS}
)

add_executable(test
    concurrent_skip_list_map_test.cc
    driver.cc
    hazard_pointer_test.cc
    metric_registry_test.cc
    metrics/counter_test.cc
    metrics/exponential_reservoir_test.cc
    metrics/meter_test.cc
    metrics/striped_int64_test.cc
    metrics/timer_test.cc
    snapshot_test.cc
    thread_local_random_test.cc
    thread_local_test.cc
)

target_link_libraries(test
    ccmetrics
    gtest
    pthread
)

add_executable(bench
    bench.cc
)

target_link_libraries(bench
    ccmetrics
    gtest
    pthread
)
