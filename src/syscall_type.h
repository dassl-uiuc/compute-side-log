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
using original_fread_t = size_t (*)(void *, size_t, size_t, FILE *);
using original_feof_t = int (*)(FILE *);
using original_fopen_t = FILE* (*)(const char *, const char *);
using original_fclose_t = int (*)(FILE *);
using original_fseek_t = int (*)(FILE *, long, int);
using original_ftell_t = int (*)(FILE *);
using original_fgets_t = char *(*)(char *, int, FILE *);
using original_fstat_t = int (*)(int, int, struct stat *);
