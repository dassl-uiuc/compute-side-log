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
#include <sys/uio.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <unordered_map>

#include "client_pool.h"
#include "csl_config.h"
#include "syscall_type.h"

// #define CSL_DEBUG
#define RECYCLE_ON_DELETE 1
#define RECOVER_FROM_REMOTE 1

#define __NEED_RECOVER_DATA(flags) (((flags)&O_TRUNC) == 0)

static original_open_t original_open = reinterpret_cast<original_open_t>(dlsym(RTLD_NEXT, "open"));
static original_open_t original_open64 = reinterpret_cast<original_open_t>(dlsym(RTLD_NEXT, "open64"));
static original_openat_t original_openat = reinterpret_cast<original_openat_t>(dlsym(RTLD_NEXT, "openat"));
static original_openat_t original_openat64 = reinterpret_cast<original_openat_t>(dlsym(RTLD_NEXT, "openat64"));
static original_write_t original_write = reinterpret_cast<original_write_t>(dlsym(RTLD_NEXT, "write"));
static original_pwrite_t original_pwrite = reinterpret_cast<original_pwrite_t>(dlsym(RTLD_NEXT, "pwrite"));
static original_pwrite_t original_pwrite64 = reinterpret_cast<original_pwrite_t>(dlsym(RTLD_NEXT, "pwrite64"));
static original_pwritev_t original_pwritev = reinterpret_cast<original_pwritev_t>(dlsym(RTLD_NEXT, "pwritev"));
static original_close_t original_close = reinterpret_cast<original_close_t>(dlsym(RTLD_NEXT, "close"));
static original_read_t original_read = reinterpret_cast<original_read_t>(dlsym(RTLD_NEXT, "read"));
static original_pread_t original_pread = reinterpret_cast<original_pread_t>(dlsym(RTLD_NEXT, "pread"));
static original_pread_t original_pread64 = reinterpret_cast<original_pread_t>(dlsym(RTLD_NEXT, "pread64"));
static original_unlink_t original_unlink = reinterpret_cast<original_unlink_t>(dlsym(RTLD_NEXT, "unlink"));
static original_lseek_t original_lseek = reinterpret_cast<original_lseek_t>(dlsym(RTLD_NEXT, "lseek"));
static original_ftruncate_t original_ftruncate = reinterpret_cast<original_ftruncate_t>(dlsym(RTLD_NEXT, "ftruncate"));
static original_ftruncate_t original_ftruncate64 =
    reinterpret_cast<original_ftruncate_t>(dlsym(RTLD_NEXT, "ftruncate64"));
static original_fsync_t original_fsync = reinterpret_cast<original_fsync_t>(dlsym(RTLD_NEXT, "fsync"));
static original_fsync_t original_fdatasync = reinterpret_cast<original_fsync_t>(dlsym(RTLD_NEXT, "fdatasync"));
static original_fread_t original_fread = reinterpret_cast<original_fread_t>(dlsym(RTLD_NEXT, "fread"));
static original_fread_t original_fread_unlocked =
    reinterpret_cast<original_fread_t>(dlsym(RTLD_NEXT, "fread_unlocked"));
static original_feof_t original_feof = reinterpret_cast<original_feof_t>(dlsym(RTLD_NEXT, "feof"));
static original_fclose_t original_fclose = reinterpret_cast<original_fclose_t>(dlsym(RTLD_NEXT, "fclose"));
static original_fopen_t original_fopen = reinterpret_cast<original_fopen_t>(dlsym(RTLD_NEXT, "fopen"));

static std::unordered_map<int, shared_ptr<CSLClient> > csl_fd_cli;
static std::unordered_map<std::string, shared_ptr<CSLClient> > csl_path_cli;
static std::mutex csl_lock;
static CSLClientPool pool;

void getClient(const char *pathname, int flags, int fd) {
    if (__IS_COMP_SIDE_LOG(flags) && (fd >= 0)) {
#ifdef CSL_DEBUG
        printf("get client for fd %d, pathname %s\n", fd, pathname);
#endif
        std::shared_ptr<CSLClient> csl_client;
        if (csl_path_cli.find(pathname) != csl_path_cli.end()) {
            csl_client = csl_path_cli[pathname];
        } else {
            csl_client = pool.GetClient(MR_SIZE, pathname, __NEED_RECOVER_DATA(flags) && RECOVER_FROM_REMOTE);
#if RECYCLE_ON_DELETE
            csl_path_cli.insert(make_pair(pathname, csl_client));
#endif
        }

        {
            std::lock_guard<std::mutex> lock(csl_lock);
            csl_fd_cli.insert(make_pair(fd, csl_client));
        }
#if RECOVER_FROM_REMOTE
#else
        if (__NEED_RECOVER_DATA(flags)) {
            csl_client->TryLocalRecover(fd);
            original_lseek(fd, 0, SEEK_SET);
        }
#endif
    }
}

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

    getClient(pathname, flags, fd);

#ifdef CSL_DEBUG
    printf("open path %s, flag 0x%x, mode 0%o\n", pathname, flags, mode);
#endif
    return fd;
}

int open64(const char *pathname, int flags, ...) {
    int fd;

    int mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, int);
        va_end(arg);

        fd = original_open64(pathname, flags, mode);
    } else {
        fd = original_open64(pathname, flags);
    }

    getClient(pathname, flags, fd);

#ifdef CSL_DEBUG
    printf("open64 path %s, flag 0x%x, mode 0%o\n", pathname, flags, mode);
#endif
    return fd;
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

    getClient(pathname, flags, fd);

#ifdef CSL_DEBUG
    printf("openat dir %d path %s, flag 0x%x, mode 0%o\n", dirfd, pathname, flags, mode);
#endif
    return fd;
}

int openat64(int dirfd, const char *pathname, int flags, ...) {
    int fd;

    mode_t mode = 0;
    if (__OPEN_NEEDS_MODE(flags)) {
        va_list arg;
        va_start(arg, flags);
        mode = va_arg(arg, mode_t);
        va_end(arg);

        fd = original_openat64(dirfd, pathname, flags, mode);
    } else {
        fd = original_openat64(dirfd, pathname, flags);
    }

    getClient(pathname, flags, fd);

#ifdef CSL_DEBUG
    printf("openat64 dir %d path %s, flag 0x%x, mode 0%o\n", dirfd, pathname, flags, mode);
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
        printf("compute side log write, fd: %d\n", fd);
#endif
        return cli->Append(buf, count);
    } else {
        csl_lock.unlock();
        return original_write(fd, buf, count);
    }
}

ssize_t pwrite_internal(int fd, const void *buf, size_t count, off_t offset, original_pwrite_t pwrite_impl) {
    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
#ifdef CSL_DEBUG
        printf("compute side log pwrite, fd: %d, size %ld, pos %ld\n", fd, count, offset);
#endif
        return cli->WritePos(buf, count, offset);
    } else {
        csl_lock.unlock();
        return pwrite_impl(fd, buf, count, offset);
    }
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite_internal(fd, buf, count, offset, original_pwrite);
}

ssize_t pwrite64(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite_internal(fd, buf, count, offset, original_pwrite64);
}

int close(int fd) {
    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        csl_lock.unlock();
#if RECYCLE_ON_DELETE
#else
        pool.RecycleClient(it->second->GetId());
#endif
#ifdef CSL_DEBUG
        printf("compute side log close, fd %d\n", fd);
#endif
        csl_lock.lock();
        csl_fd_cli.erase(it);
    }
    csl_lock.unlock();
    return original_close(fd);
}

ssize_t read(int fd, void *buf, size_t count) {
    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
#ifdef CSL_DEBUG
        printf("compute side log read, fd: %d\n", fd);
#endif
        return cli->Read(buf, count);
    } else {
        csl_lock.unlock();
        return original_read(fd, buf, count);
    }
}

ssize_t pread_internal(int fd, void *buf, size_t count, off_t offset, original_pread_t pread_impl) {
    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
#ifdef CSL_DEBUG
        printf("compute side log pread, fd: %d, pos %ld\n", fd, offset);
#endif
        return cli->ReadPos(buf, count, offset);
    } else {
        csl_lock.unlock();
        return pread_impl(fd, buf, count, offset);
    }
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    return pread_internal(fd, buf, count, offset, original_pread);
}

ssize_t pread64(int fd, void *buf, size_t count, off_t offset) {
    return pread_internal(fd, buf, count, offset, original_pread64);
}

int unlink(const char *pathname) {
#if RECYCLE_ON_DELETE
    auto it = csl_path_cli.find(pathname);
    if (it != csl_path_cli.end()) {
        pool.RecycleClient(it->second->GetId());
        csl_path_cli.erase(it);
#ifdef CSL_DEBUG
        printf("compute side log unlink, %s\n", pathname);
#endif
    }
#endif
    return original_unlink(pathname);
}

off_t lseek(int fd, off_t offset, int whence) {
    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
        return cli->Seek(offset, whence);
    } else {
        csl_lock.unlock();
        return original_lseek(fd, offset, whence);
    }
}

int ftruncate_internal(int fd, off_t length, original_ftruncate_t ftruncate_impl) {
    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
        return cli->Truncate(length);
    } else {
        csl_lock.unlock();
        return ftruncate_impl(fd, length);
    }
}

int ftruncate(int fd, off_t length) { return ftruncate_internal(fd, length, original_ftruncate); }

int ftruncate64(int fd, off_t length) { return ftruncate_internal(fd, length, original_ftruncate64); }

int sync_internal(int fd, original_fsync_t sync_impl) {
    if (csl_fd_cli.find(fd) == csl_fd_cli.end())
        return sync_impl(fd);
    else
        return 0;  // fsync/fdatasync is a no-op for NCL
}

int fsync(int fd) { return sync_internal(fd, original_fsync); }

int fdatasync(int fd) { return sync_internal(fd, original_fdatasync); }

size_t fread_internal(void *ptr, size_t size, size_t nmemb, FILE *stream, original_fread_t fread_impl) {
    int fd = fileno(stream);
    size_t count = size * nmemb;

    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();

        int ret = cli->Read(ptr, count);
#ifdef CSL_DEBUG
        printf("compute side log fread, fd: %d, ret: %d\n", fd, ret);
#endif
        return ret;
    } else {
        csl_lock.unlock();
        return fread_impl(ptr, size, nmemb, stream);
    }
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fread_internal(ptr, size, nmemb, stream, original_fread);
}

size_t fread_unlocked(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fread_internal(ptr, size, nmemb, stream, original_fread_unlocked);
}

int feof(FILE *stream) {
    int fd = fileno(stream);

    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();

        int ret = cli->Eof();
        return ret;
    } else {
        csl_lock.unlock();
        return original_feof(stream);
    }
}


int fclose(FILE *stream) {
    int fd = fileno(stream);
    if (original_fclose == nullptr)
        original_fclose = reinterpret_cast<original_fclose_t>(dlsym(RTLD_NEXT, "fclose"));
    
    if (fd < 5)  // todo: fix this workaround
        return original_fclose(stream);

    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        csl_lock.unlock();
#if RECYCLE_ON_DELETE
#else
        pool.RecycleClient(it->second->GetId());
#endif
#ifdef CSL_DEBUG
        printf("compute side log fclose, fd %d\n", fd);
#endif
        csl_lock.lock();
        csl_fd_cli.erase(it);
    }
    csl_lock.unlock();
    return original_fclose(stream);
}
