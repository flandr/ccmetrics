project(ccmetrics-library C CXX)

# Define the public include directories
set(ccmetrics_PUBLIC_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/ccmetrics
    CACHE INTERNAL "ccmetrics public include directories"
)

# The shared library target
set(ccmetrics_SHARED_LIBRARY
    ccmetrics
    CACHE INTERNAL "ccmetrics shared library"
)

# giant mega nightmare :(
set(libccmetrics_SRCS
    detail/thread_local_detail.cc
    detail/thread_local_win32.cc
    metric_registry.cc
    metrics/counter.cc
    metrics/exponential_reservoir.cc
    metrics/histogram.cc
    metrics/meter.cc
    metrics/meter_impl.cc
    metrics/striped_int64.cc
    metrics/timer.cc
    reporting/console_reporter.cc
    reporting/graphite_reporter.cc
    reporting/periodic_reporter.cc
    serializing/json_serializer.cc
    snapshot.cc
    thread_local_random.cc
)

# Set the include directories
include_directories(
    ${ccmetrics_PUBLIC_INCLUDE_DIRS}
)

# Exporting symbols
add_definitions("-DEXPORTING")

# Main library targets
add_library(${ccmetrics_SHARED_LIBRARY} SHARED ${libccmetrics_SRCS})

# Depend on rapidjson headers
add_dependencies(${ccmetrics_SHARED_LIBRARY} rapidjson_ext)

# Seems like we shouldn't have to do this explicitly :/
add_dependencies(${ccmetrics_SHARED_LIBRARY} cppformat_ext)
add_dependencies(${ccmetrics_SHARED_LIBRARY} wte_ext)

IF(WIN32)
    add_library(ccmetrics_s STATIC ${libccmetrics_SRCS})
    add_dependencies(ccmetrics_s rapidjson_ext)
    add_dependencies(ccmetrics_s cppformat_ext)
    add_dependencies(ccmetrics_s wte_ext)
    # We can combine the static libraries by passing them to the linker
    set_target_properties(ccmetrics_s PROPERTIES STATIC_LIBRARY_FLAGS ${wte_STATIC_PATH})
ENDIF (WIN32)

if(WIN32)
   set(EXTRA_LIBS ws2_32)
endif(WIN32)

# Link dependencies
target_link_libraries(${ccmetrics_SHARED_LIBRARY}
    format
    wte
    ${EXTRA_LIBS}
)

# Installation
install(TARGETS
    ${ccmetrics_SHARED_LIBRARY}
    DESTINATION lib
    EXPORT ccmetrics-targets)
install(EXPORT ccmetrics-targets DESTINATION lib)

install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/ccmetrics
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
