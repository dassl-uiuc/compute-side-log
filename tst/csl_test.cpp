#include <gtest/gtest.h>

#include <thread>
#include <memory>

#include "../src/rdma/client.h"
#include "../src/rdma/server.h"

const uint16_t PORT = 8011;

class CSLTest : public ::testing::Test {
   protected:
    void SetUp() override {
        server = new CSLServer(PORT, 1024);
        svr_th = std::thread([&]() { server->Run(); });
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
    CSLClient client({"localhost"}, PORT, 1024);
    ASSERT_TRUE(true);
}

TEST_F(CSLTest, write_test) {
    sleep(1);
    CSLClient client({"localhost"}, PORT, 1024);

    char test_buf[128];
    memset(test_buf, 42, 128);

    client.Append(test_buf, 128);
    
    char *svr_buf = reinterpret_cast<char *>(server->GetBufData());
    for (int i = 0; i < 128; i++) {
        ASSERT_EQ(svr_buf[i], 42);
    }
}

TEST_F(CSLTest, read_test) {
    sleep(1);
    CSLClient client({"localhost"}, PORT, 1024);

    memset(server->GetBufData(), 42, 128);
    client.ReadSync(0, 0, 128);

    char *cli_buf = reinterpret_cast<char *>(client.GetBufData());
    for (int i = 0; i < 128; i++) {
        ASSERT_EQ(cli_buf[i], 42);
    }
}