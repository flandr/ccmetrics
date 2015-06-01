project(ccmetrics-library C CXX)

# Define the public include directories
set(ccmetrics_PUBLIC_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/ccmetrics
    CACHE INTERNAL "ccmetrics public include directories"
)

# The shared library target
set(ccmetrics_SHARED_LIBRARY
    ccmetrics
    CACHE INTERNAL "ccmetrics shared library"
)

# giant mega nightmare :(
set(libccmetrics_SRCS
    metric_registry.cc
    metrics/counter.cc
    metrics/exponential_reservoir.cc
    metrics/meter.cc
    metrics/striped_int64.cc
    snapshot.cc
    thread_local_random.cc
)

# Set the include directories
include_directories(
    ${ccmetrics_PUBLIC_INCLUDE_DIRS}
)

# Main library targets
add_library(${ccmetrics_SHARED_LIBRARY} SHARED ${libccmetrics_SRCS})

# Apple frameworks
#IF(APPLE)
#INCLUDE_DIRECTORIES ( /Developer/Headers/FlatCarbon )
#FIND_LIBRARY(CARBON_LIBRARY Carbon)
#FIND_LIBRARY(APP_SERVICES_LIBRARY ApplicationServices )
#MARK_AS_ADVANCED (CARBON_LIBRARY
#APP_SERVICES_LIBRARY)
#SET(EXTRA_LIBS
#${CARBON_LIBRARY}
#${APP_SERVICES_LIBRARY})
#ENDIF (APPLE)

# Link dependencies
target_link_libraries(${ccmetrics_SHARED_LIBRARY}
${EXTRA_LIBS}
)

# Installation
#install(TARGETS
#${ccmetrics_SHARED_LIBRARY}
#DESTINATION lib
#EXPORT ccmetrics-targets)
#install(EXPORT ccmetrics-targets DESTINATION lib)

install(DIRECTORY
    ${CMAKE_SOURCE_DIR}/ccmetrics
    DESTINATION include/ccmetrics
    FILES_MATCHING PATTERN "*.h"
)
