#include "rdma/server.h"
#include "csl_config.h"

#include <unistd.h>
#include <thread>
#include <iostream>

using namespace std;

int main() {
    CSLServer server(PORT, MR_SIZE);

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
