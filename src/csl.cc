#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "csl.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>

using original_open_t = int (*)(const char *, int, ...);
using original_creat_t = int (*)(const char *, mode_t);
using original_openat_t = int (*)(int, const char *, int, ...);

int open(const char *pathname, int flags, ...) {
    original_open_t original_open = reinterpret_cast<original_open_t>(dlsym(RTLD_NEXT, "open"));
    int mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);

        printf("open path %s, flag 0x%x, mode 0%o\n", pathname, flags, mode);
        return original_open(pathname, flags, mode);
    } else {
        printf("open path %s, flag 0x%x, mode 0%o\n", pathname, flags, mode);
        return original_open(pathname, flags);
    }
}

int creat(const char *pathname, mode_t mode) {
    original_creat_t original_creat = reinterpret_cast<original_creat_t>(dlsym(RTLD_NEXT, "creat"));
    printf("creat path %s, mode 0%o\n", pathname, mode);
    return original_creat(pathname, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    original_openat_t original_openat = reinterpret_cast<original_openat_t>(dlsym(RTLD_NEXT, "openat"));
    mode_t mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);

        printf("openat dir %d path %s, flag 0x%x, mode 0%o\n", dirfd, pathname, flags, mode);
        return original_openat(dirfd, pathname, flags, mode);
    } else {
        printf("openat dir %d path %s, flag 0x%x, mode 0%o\n", dirfd, pathname, flags, mode);
        return original_openat(dirfd, pathname, flags);
    }
}
