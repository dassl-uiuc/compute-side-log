#include <csl.h>
#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <string>

#include "csl_config.h"

size_t MSG_SIZE = 154;
char mode = 'w';

int main(int argc, char *argv[]) {
    int i = 0;
    if (argc > 1) {
        MSG_SIZE = std::stoi(argv[1]);
        if (argc > 2) {
            mode = argv[2][0];
        }
    }
    int flags = O_RDWR | O_CREAT | O_CLOEXEC | O_CSL;
    if (mode == 'w') flags |= O_TRUNC;
    int fd = open("test.txt", flags, 0644);
    if (fd < 0) {
        std::cerr << "open file failed: " << errno << std::endl;
    }
    char *buf = new char[MSG_SIZE];
    memset(buf, 42, MSG_SIZE);

    if (mode != 'r') {
        auto start = std::chrono::high_resolution_clock::now();
        for (i = 0; i < MR_SIZE / MSG_SIZE; i++) {
            int ret;
            ret = write(fd, buf, MSG_SIZE);
            if (ret != MSG_SIZE) {
                std::cerr << "write error\n";
                exit(1);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        fdatasync(fd);

        auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "total: " << elapse << "us, average: " << static_cast<double>(elapse) / i << "us" << std::endl;
        close(fd);
    } else {
        auto start = std::chrono::high_resolution_clock::now();
        for (i = 0; i < MR_SIZE / MSG_SIZE; i++) {
            int ret = read(fd, buf, MSG_SIZE);
            if (ret != MSG_SIZE) {
                std::cerr << "read error\n";
                exit(1);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();;
        auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "total: " << elapse << "us, average: " << static_cast<double>(elapse) / i << "us" << std::endl;

        close(fd);
    }

    unlink("test.txt");
    delete buf;
}
