#include "rdma/client.h"

#include <infinity/core/Context.h>
#include <unistd.h>

#include <iostream>

#include "csl_config.h"
#include "properties.h"

int main(int argc, const char *argv[]) {
    Properties prop;
    infinity::core::Context *context = new infinity::core::Context(infinity::core::Configuration::DEFAULT_IB_DEVICE,
                                                                   infinity::core::Configuration::DEFAULT_IB_PHY_PORT);
    auto qp_pool = std::make_shared<NCLQpPool>(context, PORT);
    auto mr_pool = std::make_shared<NCLMrPool>(context);
    // CSLClient client1({"127.0.0.1"}, PORT, 1024, 0, "file1");
    CSLClient client2(qp_pool, mr_pool, ZK_DEFAULT_HOST, 1024, 1, "file2", 1, true);
    client2.SetInUse(true);

    char test_buf[128];
    memset(test_buf, 42, 128);
    int i = 0;

    // client1.Append(test_buf, 128);
    while (true) {
        sleep(1);
        client2.WritePos(test_buf, 128, 0);
        std::cout << i++ << std::endl;
    }
    char get;
    std::cin >> get;

    client2.SendFinalization();

    delete context;
    return 0;
}
