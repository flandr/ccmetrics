project(ccmetrics C CXX)

# Required CMake version
cmake_minimum_required(VERSION 2.8.11)

set (ccmetrics_MAJOR_VERSION 0)
set (ccmetrics_MINOR_VERSION 1)
set (ccmetrics_PATCH_VERSION 0)
set (ccmetrics_VERSION
    ${ccmetrics_MAJOR_VERSION}.${ccmetrics_MINOR_VERSION}.${ccmetrics_PATCH_VERSION})

# Module search path
set(CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/cmake
)

# Some CMake configuration
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)

# Default to release builds w/ debug info
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif (NOT CMAKE_BUILD_TYPE)

# Add third-party dependencies
include(ExternalProject)
include(External_cppformat)
include(External_rapidjson)
include(External_wte)

# Use C++11 when building C++ code
if (NOT WIN32)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif (NOT WIN32)

# Apple needs modern C++ libraries
IF(APPLE)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
ENDIF(APPLE)

# Recurse
add_subdirectory(src)

option(BUILD_TESTS "Build unit tests" ON)

if (BUILD_TESTS)
    include(External_gtest)
    add_subdirectory(test)
    add_subdirectory(example)
endif()

# Packaging
set (CPACK_PACKAGE_NAME libccmetrics-dev)
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fine-grained metrics for C++ applications")
set (CPACK_PACKAGE_VERSION_MAJOR ${ccmetrics_MAJOR_VERSION})
set (CPACK_PACKAGE_VERSION_MINOR ${ccmetrics_MINOR_VERSION})
set (CPACK_PACKAGE_VERSION_PATCH ${ccmetrics_PATCH_VERSION})
set (CPACK_PACKAGE_VERSION ${ccmetrics_VERSION})

set (CPACK_GENERATOR "DEB")
set (CPACK_DEBIAN_PACKAGE_MAINTAINER "Nate Rosenblum <flander@gmail.com>")
set (CPACK_DEBIAN_PACKAGE_DEPENDS "libwte-dev")

include (CPack)
