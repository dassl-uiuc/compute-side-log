#include "rdma/client.h"

const uint16_t PORT = 8011;

int main() {
    CSLClient client("127.0.0.1", PORT, 1024);

    return 0;
}
