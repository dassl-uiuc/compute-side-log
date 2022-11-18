#include "rdma/server.h"

#include <unistd.h>

const uint16_t PORT = 8011;

int main() {
    CSLServer server(PORT, 1024);

    while (true) {
        sleep(1);
    }

    return 0;
}
