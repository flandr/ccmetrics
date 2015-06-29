ExternalProject_Add(rapidjson_ext
    URL https://github.com/miloyip/rapidjson/archive/v1.0.2.tar.gz
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/rapidjson
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    BUILD_IN_SOURCE 1
)

ExternalProject_Get_Property(rapidjson_ext install_dir)
include_directories(${install_dir}/include)
link_directories(${install_dir}/lib)
