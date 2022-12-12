#include "../src/client_pool.h"

#include <gtest/gtest.h>

#include "../src/csl_config.h"
#include "../src/rdma/client.h"

class FakeCSLClient : public CSLClient {
   public:
    FakeCSLClient(set<string> host_address, uint16_t port, size_t buf_size, uint32_t id) {
        peers = host_address;
        in_use = false;
        this->buf_size = buf_size;
        this->id = id;
    }

    void Reset() override { SetInUse(false); }
};

TEST(ClientPoolTest, TestCreateCli) {
    CSLClientPool pool;
    auto cli = pool.GetClient<FakeCSLClient>(HOST_ADDRS, PORT, MR_SIZE);
    ASSERT_EQ(cli->GetPeers(), HOST_ADDRS);
    ASSERT_EQ(cli->GetBufSize(), MR_SIZE);
    ASSERT_EQ(pool.GetBusyCliCnt(), 1);
    ASSERT_EQ(pool.GetIdleCliCnt(), 0);
}

TEST(ClientPoolTest, TestRecycleCli) {
    CSLClientPool pool;
    auto cli = pool.GetClient<FakeCSLClient>(HOST_ADDRS, PORT, MR_SIZE);
    pool.RecycleClient(cli->GetId());
    ASSERT_EQ(pool.GetBusyCliCnt(), 0);
    ASSERT_EQ(pool.GetIdleCliCnt(), 1);
}

TEST(ClientPoolTest, TestReuseCli) {
    CSLClientPool pool;
    auto cli = pool.GetClient<FakeCSLClient>(HOST_ADDRS, PORT, MR_SIZE);
    pool.RecycleClient(cli->GetId());
    auto cli2 = pool.GetClient<FakeCSLClient>(HOST_ADDRS, PORT, MR_SIZE / 2);
    ASSERT_EQ(cli2, cli);
    ASSERT_EQ(pool.GetIdleCliCnt(), 0);
    ASSERT_EQ(pool.GetBusyCliCnt(), 1);
}

TEST(ClientPoolTest, TestGetCliDiffHost) {
    CSLClientPool pool;
    auto cli = pool.GetClient<FakeCSLClient>({"host1"}, PORT, MR_SIZE);
    pool.RecycleClient(cli->GetId());
    auto cli2 = pool.GetClient<FakeCSLClient>({"host2"}, PORT, MR_SIZE);
    ASSERT_NE(cli, cli2);
    ASSERT_EQ(pool.GetIdleCliCnt(), 1);
    ASSERT_EQ(pool.GetBusyCliCnt(), 1);
}

TEST(ClientPoolTest, TestGetCliBiggerMR) {
    CSLClientPool pool;
    auto cli = pool.GetClient<FakeCSLClient>(HOST_ADDRS, PORT, MR_SIZE);
    pool.RecycleClient(cli->GetId());
    auto cli2 = pool.GetClient<FakeCSLClient>(HOST_ADDRS, PORT, 2 * MR_SIZE);
    ASSERT_NE(cli, cli2);
    ASSERT_EQ(pool.GetIdleCliCnt(), 1);
    ASSERT_EQ(pool.GetBusyCliCnt(), 1);
}
