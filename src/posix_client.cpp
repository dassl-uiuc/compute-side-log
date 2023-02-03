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

int main(int argc, char *argv[]) {
    int i = 0;
    if (argc > 1) {
        MSG_SIZE = std::stoi(argv[1]);
    }
    int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_CSL, 0644);
    if (fd < 0) {
        std::cerr << "open file failed: " << errno << std::endl;
    }
    char *buf = new char[MSG_SIZE];
    memset(buf, 42, MSG_SIZE);

    auto start = std::chrono::high_resolution_clock::now();
    for (i = 0; i < MR_SIZE / MSG_SIZE; i++) {
        int ret = write(fd, buf, MSG_SIZE);
    }
    auto end = std::chrono::high_resolution_clock::now();
    fdatasync(fd);

    auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "total: " << elapse << "us, average: " << static_cast<double>(elapse) / i << "us" << std::endl;

    close(fd);

    fd = open("test2.txt", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_CSL, 0644);
    if (fd < 0) {
        std::cerr << "open file failed";
    }
    sleep(3);
    close(fd);

    // unlink("test.txt");
    // unlink("test2.txt");
    delete buf;
}
