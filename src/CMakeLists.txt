set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/csl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/client_pool.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/client.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/rdma/server.cc)

add_library(csl SHARED
    csl.h
    ${SRC_FILES})

add_executable(server server.cpp)
add_executable(client client.cpp)
add_executable(posix_client posix_client.cpp)

include_directories(${CMAKE_SOURCE_DIR}/RDMA/release/include)

target_link_libraries(csl
    PRIVATE ${CMAKE_SOURCE_DIR}/RDMA/release/libinfinity.a
    PRIVATE ibverbs
    glog::glog
    PUBLIC Threads::Threads)

target_link_libraries(server csl)
target_link_libraries(client csl)
target_link_libraries(posix_client csl)
