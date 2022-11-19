#include "rdma/client.h"

const uint16_t PORT = 8011;

int main() {
    CSLClient client("127.0.0.1", PORT, 1024);

    char test_buf[128];
    memset(test_buf, 42, 128);

    client.Append(test_buf, 128);

    return 0;
}
