cmake_minimum_required(VERSION 3.14)

project(compute_side_log)
set(CMAKE_CXX_STANDARD 17)

enable_testing()

include(FetchContent)

FetchContent_Declare(
    glog
    GIT_REPOSITORY https://github.com/google/glog.git
    GIT_TAG v0.6.0
)
FetchContent_MakeAvailable(glog)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

find_package(Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-result")

add_subdirectory(RDMA)
add_subdirectory(HdrHistogram_c)
add_subdirectory(src)
add_subdirectory(tst)
