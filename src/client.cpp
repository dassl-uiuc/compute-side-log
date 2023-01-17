#include "rdma/client.h"
#include "csl_config.h"
#include <iostream>

int main() {
    // CSLClient client1({"127.0.0.1"}, PORT, 1024, 0, "file1");
    CSLClient client2(ZK_DEFAULT_HOST, 1024, 1, "file2");

    char test_buf[128];
    memset(test_buf, 42, 128);

    // client1.Append(test_buf, 128);
    client2.Append(test_buf, 128);
    char get;
    std::cin >> get;
    
    return 0;
}
