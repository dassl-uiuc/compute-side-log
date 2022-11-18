set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/csl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/client.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/server.cc)

add_library(csl SHARED
    csl.h
    ${SRC_FILES})

add_executable(server server.cpp)
add_executable(client client.cpp)

include_directories(${CMAKE_SOURCE_DIR}/RDMA/release/include)

target_link_libraries(csl
    ${CMAKE_SOURCE_DIR}/RDMA/release/libinfinity.so
    ibverbs
    glog::glog)

target_link_libraries(server csl)

target_link_libraries(client csl)
