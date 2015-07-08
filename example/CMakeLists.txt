project(ccmetrics-examples C CXX)

include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

add_executable(example
    example.cc
)

target_link_libraries(example
    ccmetrics
)

IF(!WIN32)
    target_link_libraries(example pthread)
ENDIF(!WIN32)
