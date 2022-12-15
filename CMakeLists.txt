cmake_minimum_required(VERSION 3.14)

project(compute_side_log)
set(CMAKE_CXX_STANDARD 14)

enable_testing()

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

find_package(glog)
find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG -pg)

add_subdirectory(RDMA)
add_subdirectory(src)
add_subdirectory(tst)
