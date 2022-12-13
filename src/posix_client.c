#include "csl.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    int fd = open("test.txt", O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_CSL, 0644);
    if (fd < 0) {
        printf("open file failed\n");
    }
    char *buf = malloc(128);
    memset(buf, 42, 128);
    int ret = write(fd, buf, 128);

    free(buf);
    close(fd);
}
