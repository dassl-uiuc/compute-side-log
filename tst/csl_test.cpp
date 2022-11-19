#include <gtest/gtest.h>

#include <thread>

#include "../src/rdma/client.h"
#include "../src/rdma/server.h"

const uint16_t PORT = 8011;

class CSLTest : public ::testing::Test {
   protected:
    void SetUp() override {
        svr_th = std::thread([&]() { server = new CSLServer(PORT, 1024); });
    }

    void TearDown() override {
        svr_th.join();
        delete server;
    }
    CSLServer *server;
    std::thread svr_th;
};

TEST_F(CSLTest, basic_test) {
    sleep(1);
    CSLClient client("localhost", PORT, 1024);
    ASSERT_TRUE(true);
}

TEST_F(CSLTest, write_test) {
    sleep(1);
    CSLClient client("localhost", PORT, 1024);

    char test_buf[128];
    memset(test_buf, 42, 128);

    client.Append(test_buf, 128);
    
    char *svr_buf = (char *)server->GetBufData();
    for (int i = 0; i < 128; i++) {
        ASSERT_EQ(svr_buf[i], 42);
    }
}
