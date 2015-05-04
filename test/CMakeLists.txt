project(tests C CXX)

# Set includes
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/test
    ${gtest_INCLUDE_DIRS}
)

add_executable(test
    driver.cc
    thread_local_test.cc
)

target_link_libraries(test
    #ccmetrics
    gtest
    pthread
)
