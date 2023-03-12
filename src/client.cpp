#include "rdma/client.h"
#include "csl_config.h"
#include <iostream>

#include <infinity/core/Context.h>

int main() {
    infinity::core::Context *context = new infinity::core::Context(infinity::core::Configuration::DEFAULT_IB_DEVICE, 1);
    auto qp_pool = std::make_shared<NCLQpPool>(context, PORT);
    auto mr_pool = std::make_shared<NCLMrPool>(context);
    // CSLClient client1({"127.0.0.1"}, PORT, 1024, 0, "file1");
    CSLClient client2(qp_pool, mr_pool, ZK_DEFAULT_HOST, 1024, 1, "file2");
    client2.SetInUse(true);

    char test_buf[128];
    memset(test_buf, 42, 128);

    // client1.Append(test_buf, 128);
    client2.Append(test_buf, 128);
    char get;
    std::cin >> get;

    client2.SendFinalization();

    delete context;    
    return 0;
}
