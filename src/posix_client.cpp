#include <csl.h>
#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>

size_t MSG_SIZE = 154;
size_t TOTAL_SIZE = 100;
char mode = 'w';
std::string filename = "test.txt";
bool do_sync = false;

/**
 * Usage:
 * ./posix_client <msg_size> w/r <filename> [ncl/direct/prepare/sync]
 */
int main(int argc, char *argv[]) {
    int i = 0;
    int flags = O_RDWR | O_CREAT | O_CLOEXEC;
    if (argc > 1) {
        MSG_SIZE = std::stoul(argv[1]);
        if (argc > 2) {
            mode = argv[2][0];
        }
        if (argc > 3) {
            filename = argv[3];
        }
        if (argc > 4) {
            if (strcmp(argv[4], "ncl") == 0)
                flags |= O_CSL;
            else if (strcmp(argv[4], "direct") == 0)
                flags |= O_DIRECT;
            else if (strcmp(argv[4], "prepare") == 0)
                flags |= O_CSL;
            else if (strcmp(argv[4], "sync") == 0)
                do_sync = true;
        }
        if (argc > 5) {
            TOTAL_SIZE = std::stoul(argv[5]);
        }
    }
    TOTAL_SIZE *= 1048576;

    std::cout << "msg size: " << MSG_SIZE << "B\ntotal size: " << TOTAL_SIZE << "\nmode: " << mode
              << "\nfilename: " << filename << "\ntype: " << (argc > 4 ? argv[4] : "normal") << std::endl;

    if (mode == 'w') flags |= O_TRUNC;
    int fd = open(filename.c_str(), flags, 0644);
    if (fd < 0) {
        std::cerr << "open file failed: " << errno << std::endl;
    }
    char *buf = new char[MSG_SIZE];
    memset(buf, 42, MSG_SIZE);

    if (mode != 'r') {
        auto start = std::chrono::high_resolution_clock::now();
        for (i = 0; i < (TOTAL_SIZE - sizeof(uint64_t)) / MSG_SIZE; i++) {  // do not overwrite sequence number
            int ret;
            ret = write(fd, buf, MSG_SIZE);
            if (ret != MSG_SIZE) {
                std::cerr << "write error\n";
                exit(1);
            }
            if (do_sync)
                fdatasync(fd);
        }
        auto end = std::chrono::high_resolution_clock::now();
        fdatasync(fd);

        auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "total: " << elapse << " us\nnum: " << i << "\naverage: " << static_cast<double>(elapse) / i
                  << " us" << std::endl;
        close(fd);
    } else {
        auto start = std::chrono::high_resolution_clock::now();
        for (i = 0; i < (TOTAL_SIZE - sizeof(uint64_t)) / MSG_SIZE; i++) {
            int ret = read(fd, buf, MSG_SIZE);
            if (ret != MSG_SIZE) {
                std::cerr << i << " " << ret << " read error\n";
                exit(1);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "total: " << elapse << " us\nnum: " << i << "\naverage: " << static_cast<double>(elapse) / i
                  << " us" << std::endl;

        close(fd);
    }

    if ((argc > 4 && strcmp(argv[4], "ncl") == 0)) {
        unlink(filename.c_str());
    }
    delete buf;
}
