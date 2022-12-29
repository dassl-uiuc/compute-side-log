#include "rdma/server.h"
#include "csl_config.h"

#include <unistd.h>
#include <thread>
#include <iostream>

using namespace std;

int main() {
    CSLServer server(PORT, MR_SIZE, "localhost");

    thread svr_th = thread([&]() {
        server.Run();
    });

    while (true) {
        sleep(1);
        cout << "total client: " << server.GetConnectionCount() << endl;
        vector<string> all_files = server.GetAllFilename();
        for (auto &f : all_files) {
            cout << "file " << f << endl;
            char *buf = (char *)server.GetBufData(f);
            for (int j = 0; j < 128; j++)
                cout << buf[j];
            cout << endl;
        }
    }

    return 0;
}
