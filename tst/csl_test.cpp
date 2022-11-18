#include <gtest/gtest.h>

#include <thread>

#include "../src/rdma/client.h"
#include "../src/rdma/server.h"

const uint16_t PORT = 8011;

class CSLTest : public ::testing::Test {
   protected:
    void SetUp() override {
        std::thread svr_th([&]() { server = new CSLServer(PORT, 1024); });
        svr_th.detach();
    }

    void TearDown() override { delete server; }
    CSLServer *server;
};

TEST_F(CSLTest, basic_test) {
    sleep(1);
    CSLClient client("localhost", PORT, 1024);
    ASSERT_TRUE(true);
}
