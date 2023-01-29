/*
 * glic file operation stubs for compute-side log
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "csl.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include <mutex>
#include <unordered_map>

#include "client_pool.h"
#include "csl_config.h"

// #define CSL_DEBUG

using original_open_t = int (*)(const char *, int, ...);
using original_creat_t = int (*)(const char *, mode_t);
using original_openat_t = int (*)(int, const char *, int, ...);
using original_write_t = ssize_t (*)(int, const void *, size_t);
using original_close_t = int (*)(int);

static original_open_t original_open = reinterpret_cast<original_open_t>(dlsym(RTLD_NEXT, "open"));
static original_creat_t original_creat = reinterpret_cast<original_creat_t>(dlsym(RTLD_NEXT, "creat"));
static original_openat_t original_openat = reinterpret_cast<original_openat_t>(dlsym(RTLD_NEXT, "openat"));
static original_write_t original_write = reinterpret_cast<original_write_t>(dlsym(RTLD_NEXT, "write"));
static original_close_t original_close = reinterpret_cast<original_close_t>(dlsym(RTLD_NEXT, "close"));

static std::unordered_map<int, shared_ptr<CSLClient> > csl_fd_cli;
static std::mutex csl_lock;
static CSLClientPool pool;

int open(const char *pathname, int flags, ...) {
    int fd;

    int mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);

        fd = original_open(pathname, flags, mode);
    } else {
        fd = original_open(pathname, flags);
    }

    if (__IS_COMP_SIDE_LOG(flags) && fd >= 0) {
        auto csl_client = pool.GetClient(MR_SIZE, pathname);
        {
            std::lock_guard<std::mutex> lock(csl_lock);
            csl_fd_cli.insert(make_pair(fd, csl_client));
        }
    }

#ifdef CSL_DEBUG
    printf("open path %s, flag 0x%x, mode 0%o\n", pathname, flags, mode);
#endif
    return fd;
}

int creat(const char *pathname, mode_t mode) {
#ifdef CSL_DEBUG
    printf("creat path %s, mode 0%o\n", pathname, mode);
#endif
    return original_creat(pathname, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    int fd;

    mode_t mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);

        fd = original_openat(dirfd, pathname, flags, mode);
    } else {
        fd = original_openat(dirfd, pathname, flags);
    }

    if (__IS_COMP_SIDE_LOG(flags) && fd >= 0) {
        auto csl_client = pool.GetClient(MR_SIZE, pathname);
        {
            std::lock_guard<std::mutex> lock(csl_lock);
            csl_fd_cli.insert(make_pair(fd, csl_client));
        }
    }

#ifdef CSL_DEBUG
    printf("openat dir %d path %s, flag 0x%x, mode 0%o\n", dirfd, pathname, flags, mode);
#endif
    return fd;
}

ssize_t write(int fd, const void *buf, size_t count) {
    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
#ifdef CSL_DEBUG
        printf("compute side log, fd: %d\n", fd);
#endif
        cli->Append(buf, count);
    } else {
        csl_lock.unlock();
    }
    return original_write(fd, buf, count);
}

int close(int fd) {
    std::lock_guard<std::mutex> lock(csl_lock);
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        pool.RecycleClient(csl_fd_cli[fd]->GetId());
        csl_fd_cli.erase(fd);
    }
    return original_close(fd);
}
