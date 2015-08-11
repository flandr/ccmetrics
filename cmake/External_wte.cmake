ExternalProject_Add(wte_ext
    URL https://github.com/flandr/what-the-event/archive/v0.1.0-alpha.4.tar.gz
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/wte
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBUILD_TESTS=off
    BUILD_IN_SOURCE 1
)

ExternalProject_Get_Property(wte_ext install_dir)
include_directories(${install_dir}/include)
link_directories(${install_dir}/lib)

if(WIN32)
    set(wte_STATIC_PATH ${install_dir}/lib/wte_s.lib)
endif(WIN32)
