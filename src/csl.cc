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
#include <sys/stat.h>
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
static original_fopen_t original_fopen64 = reinterpret_cast<original_fopen_t>(dlsym(RTLD_NEXT, "fopen64"));
static original_fseek_t original_fseek = reinterpret_cast<original_fseek_t>(dlsym(RTLD_NEXT, "fseek"));
static original_ftell_t original_ftello64 = reinterpret_cast<original_ftell_t>(dlsym(RTLD_NEXT, "ftello64"));
static original_fgets_t original_fgets = reinterpret_cast<original_fgets_t>(dlsym(RTLD_NEXT, "fgets"));
static original_fstat64_t original_fstat64 = reinterpret_cast<original_fstat64_t>(dlsym(RTLD_NEXT, "__fxstat64"));
static original_stat64_t original_stat64 = reinterpret_cast<original_stat64_t>(dlsym(RTLD_NEXT, "__xstat64"));

static std::unordered_map<int, shared_ptr<CSLClient> > csl_fd_cli;
static std::unordered_map<std::string, shared_ptr<CSLClient> > csl_path_cli;
static std::mutex csl_lock;
static CSLClientPool pool;

/*
 * This struct is to tell whether the static variables such as the hash maps have been constructed or evaluated, if not,
 * we shall not use them. Otherwise using a uninitialized hash map will raise an exception. This is because although the
 * hash maps above are declared as static variables and are initialized before program entry point, some glibc functions
 * such fclose are called even earlier than the static variables are initialized. At that time we must not access the
 * hash map (since it's uninitialized) and there is no need to check the hash map as there is no NCL logs opened at that
 * time. Instead, we directly use the original implementation of those glibc functions.
 */
static struct InitializeIndicator {
    bool initialized;
    InitializeIndicator() { initialized = true; }
} init_d;

void getClient(const char *pathname, int flags, int fd) {
    if (__IS_COMP_SIDE_LOG(flags)) {
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

        if (fd >= 0) {
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

    if (fd >= 0) getClient(pathname, flags, fd);

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

    if (fd >= 0) getClient(pathname, flags, fd);

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

    if (fd >= 0) getClient(pathname, flags, fd);

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

    if (fd >= 0) getClient(pathname, flags, fd);

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

int fseek(FILE *stream, long offset, int whence) {
    int fd = fileno(stream);

    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
#ifdef CSL_DEBUG
        printf("compute side log fseek, fd %d, offset %ld, whence %d\n", fd, offset, whence);
#endif
        return cli->Seek(offset, whence);
    } else {
        csl_lock.unlock();
        return original_fseek(stream, offset, whence);
    }
}

off_t ftello64(FILE *stream) {
    int fd = fileno(stream);

    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
        size_t pos = cli->GetOffset();
#ifdef CSL_DEBUG
        printf("compute side log ftello64, fd %d, pos %ld\n", fd, pos);
#endif
        return pos;
    } else {
        csl_lock.unlock();
        return original_ftello64(stream);
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

char *fgets(char *s, int size, FILE *stream) {
    int fd = fileno(stream);

    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
#ifdef CSL_DEBUG
        printf("compute side log fgets, fd: %d\n", fd);
#endif
        return cli->GetLine(s, size);
    } else {
        csl_lock.unlock();
        return original_fgets(s, size, stream);
    }
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

FILE *fopen_internal(const char *pathname, const char *mode, original_fopen_t fopen_impl) {
    FILE *fp = fopen_impl(pathname, mode);

    if (fp) {
        int fd = fileno(fp);
        int mode_len = strlen(mode);
        if (mode[mode_len - 1] == 'l') {
#ifdef CSL_DEBUG
            printf("compute side log fopen, fd %d, path %s, mode %s\n", fd, pathname, mode);
#endif
            getClient(pathname, O_CSL, fd);
        }
    }

    return fp;
}

FILE *fopen(const char *pathname, const char *mode) {
    if (original_fopen == nullptr) {
        original_fopen = reinterpret_cast<original_fopen_t>(dlsym(RTLD_NEXT, "fopen"));
    }
    return fopen_internal(pathname, mode, original_fopen);
}

FILE *fopen64(const char *pathname, const char *mode) {
    if (original_fopen64 == nullptr) {
        original_fopen64 = reinterpret_cast<original_fopen_t>(dlsym(RTLD_NEXT, "fopen64"));
    }
    return fopen_internal(pathname, mode, original_fopen64);
}

int fclose(FILE *stream) {
    int fd = fileno(stream);
    if (original_fclose == nullptr) original_fclose = reinterpret_cast<original_fclose_t>(dlsym(RTLD_NEXT, "fclose"));

    if (!init_d.initialized) {
        return original_fclose(stream);
    }

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

int __fxstat64(int vers, int fd, struct stat64 *buf) {
    if (original_fstat64 == nullptr) {
        original_fstat64 = reinterpret_cast<original_fstat64_t>(dlsym(RTLD_NEXT, "__fxstat64"));
    }

    int ret = original_fstat64(vers, fd, buf);

    csl_lock.lock();
    auto it = csl_fd_cli.find(fd);
    if (it != csl_fd_cli.end()) {
        auto cli = it->second;
        csl_lock.unlock();
        buf->st_size = cli->GetFileSize();
#ifdef CSL_DEBUG
        printf("compute side log fstat, fd %d, size %ld\n", fd, buf->st_size);
#endif
    } else {
        csl_lock.unlock();
    }

    return ret;
}

int __xstat64(int vers, const char *name, struct stat64 *buf) {
    if (original_stat64 == nullptr) {
        original_stat64 = reinterpret_cast<original_stat64_t>(dlsym(RTLD_NEXT, "__xstat64"));
    }

    int ret = original_stat64(vers, name, buf);
    if (ret < 0)
        return ret;
    auto it = csl_path_cli.find(name);
    if (it != csl_path_cli.end()) {
        auto cli = it->second;
        buf->st_size = cli->GetFileSize();
#ifdef CSL_DEBUG
        printf("compute side log stat, path %s, size %ld\n", name, buf->st_size);
#endif
    } else if (buf->st_size == 0) {
        /* TODO:
         * This is a heuristic approach, if a file has an entry in the fs but its size is 0, then it's probably a NCL
         * file. In this case we prefetch its content from peers.
         */
        getClient(name, O_CSL, -1);
        buf->st_size = csl_path_cli[name]->GetFileSize();
#ifdef CSL_DEBUG
        printf("compute side log stat, path %s, size %ld\n", name, buf->st_size);
#endif
    }

    return ret;
}
