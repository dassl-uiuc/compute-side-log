#include "rdma/server.h"

#include <unistd.h>

#include <csignal>
#include <iostream>
#include <thread>

#include "csl_config.h"

using namespace std;

bool stop = false;

void signal_handler(int signal) { stop = true; }

int main() {
    signal(SIGINT, signal_handler);
    CSLServer server(PORT, MR_SIZE, ZK_DEFAULT_HOST);

    thread svr_th = thread([&]() { server.Run(); });

    while (!stop) {
        sleep(1);
        cout << "total client: " << server.GetConnectionCount() << endl;
        vector<string> all_files = server.GetAllFileId();
        for (auto &f : all_files) {
            cout << "file " << f << endl;
            char *buf = (char *)server.GetBufData(f);
            for (int j = 0; j < 128; j++)
                cout << buf[j];
            cout << endl;
        }
    }

    server.Stop();
    svr_th.join();

    return 0;
}
