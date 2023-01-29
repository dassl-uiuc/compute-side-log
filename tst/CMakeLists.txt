add_executable(csl_test
    # client_pool_test.cpp
    util_test.cpp)

target_include_directories(csl_test
    PRIVATE ${CMAKE_SOURCE_DIR}/RDMA/release/include)

target_link_libraries(csl_test
    GTest::gtest_main
    csl
    zookeeper_mt)

include(GoogleTest)
gtest_discover_tests(csl_test)
