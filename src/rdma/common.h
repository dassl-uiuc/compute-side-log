/*
 * common data structures for Compute-side log RDMA client and server
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#pragma once

#include <stddef.h>

#define CLOSE_FILE 1

struct FileInfo {
    size_t size;
    char file_id[512];
}__attribute__((packed));

struct ClientReq {
    int type;
    char file_id[512];
}__attribute__((packed));
