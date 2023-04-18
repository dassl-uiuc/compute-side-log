#pragma once

#include <set>
#include <string>

using original_open_t = int (*)(const char *, int, ...);
// using original_creat_t = int (*)(const char *, mode_t);
using original_openat_t = int (*)(int, const char *, int, ...);
using original_write_t = ssize_t (*)(int, const void *, size_t);
using original_pwrite_t = ssize_t (*)(int, const void *, size_t, off_t);
using original_pwritev_t = ssize_t (*)(int, const struct iovec *, int, off_t);
using original_close_t = int (*)(int);
using original_read_t = ssize_t (*)(int, void *, size_t);
using original_pread_t = ssize_t (*)(int, void *, size_t, off_t);
using original_unlink_t = int (*)(const char *);
using original_lseek_t = off_t (*)(int, off_t, int);

const uint16_t PORT = 8011;  // data plane service port
const std::string ZK_DEFAULT_HOST = "127.0.0.1:2181";
const std::string ZK_SVR_ROOT_PATH = "/servers";
const std::string ZK_CLI_ROOT_PATH = "/clients";
const int DEFAULT_REP_FACTOR = 1;
const size_t MR_SIZE = 1024 * 1024 * 100;
const char TAIL_MARKER = 255;  // a magic number
const std::set<std::string> HOST_ADDRS = {
    "localhost"
};
