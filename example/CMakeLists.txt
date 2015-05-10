project(ccmetrics-examples C CXX)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

add_executable(example
    example.cc
)

target_link_libraries(example
    ccmetrics
    pthread
)
