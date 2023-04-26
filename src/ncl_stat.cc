#include <dlfcn.h>
#include <fcntl.h>
#include <hdr/hdr_histogram.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include "syscall_type.h"

using original_fsync_t = int (*)(int);
using original_fopen_t = FILE *(*)(const char *, const char *);
using original_fwrite_t = size_t (*)(const void *, size_t, size_t, FILE *);
using original_fclose_t = int (*)(FILE *);

static original_open_t original_open = reinterpret_cast<original_open_t>(dlsym(RTLD_NEXT, "open"));
static original_open_t original_open64 = reinterpret_cast<original_open_t>(dlsym(RTLD_NEXT, "open64"));
static original_openat_t original_openat = reinterpret_cast<original_openat_t>(dlsym(RTLD_NEXT, "openat"));
static original_openat_t original_openat64 = reinterpret_cast<original_openat_t>(dlsym(RTLD_NEXT, "openat64"));
static original_write_t original_write = reinterpret_cast<original_write_t>(dlsym(RTLD_NEXT, "write"));
static original_pwrite_t original_pwrite = reinterpret_cast<original_pwrite_t>(dlsym(RTLD_NEXT, "pwrite"));
static original_pwrite_t original_pwrite64 = reinterpret_cast<original_pwrite_t>(dlsym(RTLD_NEXT, "pwrite64"));
static original_pwritev_t original_pwritev = reinterpret_cast<original_pwritev_t>(dlsym(RTLD_NEXT, "pwritev"));
static original_close_t original_close = reinterpret_cast<original_close_t>(dlsym(RTLD_NEXT, "close"));
static original_fsync_t original_fdatasync = reinterpret_cast<original_fsync_t>(dlsym(RTLD_NEXT, "fdatasync"));
static original_fsync_t original_fsync = reinterpret_cast<original_fsync_t>(dlsym(RTLD_NEXT, "fsync"));
static original_fopen_t original_fopen = reinterpret_cast<original_fopen_t>(dlsym(RTLD_NEXT, "fopen"));
static original_fopen_t original_fopen64 = reinterpret_cast<original_fopen_t>(dlsym(RTLD_NEXT, "fopen64"));
static original_fwrite_t original_fwrite = reinterpret_cast<original_fwrite_t>(dlsym(RTLD_NEXT, "fwrite"));
static original_fclose_t original_fclose = reinterpret_cast<original_fclose_t>(dlsym(RTLD_NEXT, "fclose"));

using namespace std;

const string LOG = "aof";
const string DB = "rdb";

struct IoStat {
    const set<string> exts = {LOG, DB};
    unordered_map<string, hdr_histogram *> ext_stat;
    unordered_map<int, hdr_histogram *> fd_stat;
    unordered_map<int, size_t> fd_accumulated;

    IoStat() {
        for (auto &e : exts) {
            hdr_init(1, 1024 * 1024 * 1024, 3, &ext_stat[e]);
        }
    }

    ~IoStat() {
        show(LOG);
    }

    void show(const string &e) {
        cout << "ext: " << e << " pid: " << getpid() << endl;
        string stat_name = e + "_stat.csv";
        FILE *fp = fopen(stat_name.c_str(), "w");
        for (int i = 0; i <= 100; i++) {
            printf("%d,%ld\n", i, hdr_value_at_percentile(ext_stat[e], (double)i));
        }
        fclose(fp);
        hdr_percentiles_print(ext_stat[e], stdout, 5, 1, CLASSIC);
    }
};

static IoStat io_stat = {};
static mutex lk;

string getFileExt(const string &s) {
    size_t i = s.rfind('.', s.length());
    if (i != string::npos) {
        return (s.substr(i + 1, s.length() - i));
    }

    return ("");
}

void prepareHist(const char *path, int fd) {
    // lock_guard<mutex> gd(lk);
    string ext = getFileExt(path);
    if (io_stat.exts.find(ext) != io_stat.exts.end()) {
        io_stat.fd_stat[fd] = io_stat.ext_stat[ext];
        cout << "pid " << getpid() << " opened: " << path << ", fd: " << fd << ", extension: " << ext << endl;
        if (ext.compare(DB) == 0 || ext.compare(LOG) == 0) {
            io_stat.fd_accumulated[fd] = 0;
            cout << path << " is db file" << endl;
        }
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

    prepareHist(pathname, fd);

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

    prepareHist(pathname, fd);

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

    prepareHist(pathname, fd);

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

    prepareHist(pathname, fd);

    return fd;
}

FILE *fopen_internal(const char *pathname, const char *mode, original_fopen_t fopen_impl) {
    FILE *fp = fopen_impl(pathname, mode);
    if (fp) {
        int fd = fileno(fp);

        prepareHist(pathname, fd);
    }
    return fp;
}

FILE *fopen(const char *pathname, const char *mode) { return fopen_internal(pathname, mode, original_fopen); }

FILE *fopen64(const char *pathname, const char *mode) { return fopen_internal(pathname, mode, original_fopen64); }

void record_value(int fd, size_t count) {
    // lock_guard<mutex> gd(lk);
    if (io_stat.fd_stat.find(fd) != io_stat.fd_stat.end()) {
        if (io_stat.fd_accumulated.find(fd) != io_stat.fd_accumulated.end()) {
            io_stat.fd_accumulated[fd] += count;
        } else {
            hdr_record_value(io_stat.fd_stat[fd], count);
        }
    }
}

bool record_accum_value(int fd) {
    // lock_guard<mutex> gd(lk);
    if ((io_stat.fd_accumulated.find(fd) != io_stat.fd_accumulated.end()) && (io_stat.fd_accumulated[fd] > 0)) {
        hdr_record_value(io_stat.fd_stat[fd], io_stat.fd_accumulated[fd]);
        
        if (io_stat.fd_stat[fd] != io_stat.ext_stat[LOG])
        cout << "------------------"<<fd<<"-------------------- accum write: " << io_stat.fd_accumulated[fd] << endl;
        
        io_stat.fd_accumulated[fd] = 0;
        return true;
    }

    return false;
}

ssize_t write(int fd, const void *buf, size_t count) {
    record_value(fd, count);
    return original_write(fd, buf, count);
}

ssize_t pwrite_internal(int fd, const void *buf, size_t count, off_t offset, original_pwrite_t pwrite_impl) {
    record_value(fd, count);
    return pwrite_impl(fd, buf, count, offset);
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite_internal(fd, buf, count, offset, original_pwrite);
}

ssize_t pwrite64(int fd, const void *buf, size_t count, off_t offset) {
    return pwrite_internal(fd, buf, count, offset, original_pwrite64);
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    int fd = fileno(stream);
    record_value(fd, size * nmemb);
    return original_fwrite(ptr, size, nmemb, stream);
}

int sync_internal(int fd, original_fsync_t sync_impl) {
    if (record_accum_value(fd))
        return 0;
    else
        return sync_impl(fd);
}

int fsync(int fd) { return sync_internal(fd, original_fsync); }

int fdatasync(int fd) { return sync_internal(fd, original_fdatasync); }

int close(int fd) {
    {
        // lock_guard<mutex> gd(lk);
        io_stat.fd_stat.erase(fd);
        io_stat.fd_accumulated.erase(fd);
    }
    return original_close(fd);
}

int fclose(FILE *stream) {
    io_stat.fd_stat.erase(fileno(stream));
    io_stat.fd_accumulated.erase(fileno(stream));
    return original_fclose(stream);
}
