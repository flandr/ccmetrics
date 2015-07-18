ExternalProject_Add(cppformat_ext
    URL https://github.com/cppformat/cppformat/releases/download/1.1.0/cppformat-1.1.0.zip
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}/cppformat
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR> -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DCMAKE_POSITION_INDEPENDENT_CODE=true
    BUILD_IN_SOURCE 1
)

ExternalProject_Get_Property(cppformat_ext install_dir)
include_directories(${install_dir}/include)
link_directories(${install_dir}/lib)
