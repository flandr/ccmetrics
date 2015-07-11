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
    reporting_test.cc
    serializing_test.cc
    snapshot_test.cc
    thread_local_random_test.cc
    thread_local_test.cc
)

target_link_libraries(test
    gtest
)

IF(WIN32)
    target_link_libraries(test ccmetrics_s)
ELSE(WIN32)
    target_link_libraries(test ccmetrics)
ENDIF(WIN32)

IF(NOT WIN32)
    target_link_libraries(test pthread)
ENDIF(NOT WIN32)

IF(NOT WIN32)
add_executable(bench
    bench.cc
)
target_link_libraries(bench
    ccmetrics
    pthread
)
ENDIF(NOT WIN32)
