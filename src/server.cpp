#include "rdma/server.h"

#include <unistd.h>
#include <thread>
#include <iostream>

using namespace std;

const uint16_t PORT = 8011;

int main() {
    CSLServer server(PORT, 1024);

    thread svr_th = thread([&]() {
        server.Run();
    });

    while (true) {
        sleep(1);
        for (int i = 0; i < server.GetConnectionCount(); i++) {
            cout << "client " << i << endl;
            char *buf = (char *)server.GetBufData(i);
            for (int j = 0; j < 128; j++)
                cout << buf[j] << " ";
            cout << endl;
        }
    }

    return 0;
}
