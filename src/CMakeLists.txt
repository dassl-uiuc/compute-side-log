set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/csl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/client_pool.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/client.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/server.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/qp_pool.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/mr_pool.cc)

add_library(csl SHARED
    csl.h
    ${SRC_FILES})

add_library(nclstat SHARED
    ncl_stat.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/ncl_stat.cc)

target_include_directories(nclstat
    PRIVATE ${CMAKE_SOURCE_DIR}/HdrHistogram_c/include)

target_link_libraries(nclstat
    PRIVATE hdr_histogram_static
    ${CMAKE_DL_LIBS})

add_executable(server server.cpp properties.cc)
add_executable(client client.cpp properties.cc)
add_executable(posix_client posix_client.cpp)

include_directories(${CMAKE_SOURCE_DIR}/RDMA/release/include)

target_compile_definitions(server PUBLIC -DTHREADED)

target_link_libraries(csl
    PRIVATE ${CMAKE_SOURCE_DIR}/RDMA/release/libinfinity.a
    PRIVATE ibverbs
    PRIVATE zookeeper_mt
    PUBLIC glog::glog
    PUBLIC Threads::Threads)

target_link_libraries(server csl)
target_link_libraries(client csl)
target_link_libraries(posix_client csl)
