/*
 * common data structures for Compute-side log RDMA client and server
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#pragma once

#include <stddef.h>

#define OPEN_FILE   1
#define CLOSE_FILE  2
#define EXIT_PROC   3
#define GET_INFO    4
#define SYNC_PEER   5
#define SYNC_PEER_DONE  6

#define MAX_FILE_ID_LENGTH 512

struct FileInfo {
    size_t size;
    char file_id[MAX_FILE_ID_LENGTH];
}__attribute__((packed));

struct ClientReq {
    int type;
    FileInfo fi;
}__attribute__((packed));

struct ServerResp {
    size_t size;
    uint64_t seq;
};
