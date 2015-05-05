project(tests C CXX)

# Set includes
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${gtest_INCLUDE_DIRS}
)

add_executable(test
    driver.cc
    hazard_pointer_test.cc
    metrics/striped_int64_test.cc
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
