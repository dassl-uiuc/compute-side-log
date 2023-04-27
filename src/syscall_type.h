#pragma once

#include <sys/types.h>

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
using original_ftruncate_t = int (*)(int, off_t);
using original_fsync_t = int (*)(int);
