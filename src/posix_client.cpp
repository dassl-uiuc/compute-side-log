#include "csl.h"
#include "csl_config.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <error.h>
#include <iostream>
#include <chrono>

const size_t MSG_SIZE = 154;

int main() {
    int i = 0;
    int fd = open("test.txt", O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_CSL, 0644);
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

    auto elapse = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::cout << "total: " << elapse << "us, average: " << static_cast<double>(elapse)/i << "us" << std::endl;

    delete buf;
    close(fd);
}
