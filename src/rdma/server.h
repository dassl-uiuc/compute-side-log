/*
 * Compute-side log RDMA server
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#pragma once

#ifndef THREADED
#define THREADED
#endif

#include <infinity/core/Configuration.h>
#include <infinity/core/Context.h>
#include <infinity/memory/Buffer.h>
#include <infinity/memory/RegionToken.h>
#include <infinity/queues/QueuePair.h>
#include <infinity/queues/QueuePairFactory.h>
#include <zookeeper/zookeeper.h>

#include <fstream>
#include <unordered_map>

#include "../csl_config.h"
#include "mr_pool.h"

using namespace std;
using infinity::queues::QueuePair;
using infinity::memory::Buffer;
using infinity::memory::RegionToken;
using infinity::queues::QueuePairFactory;

class CSLServer {
    friend void ServerWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
    struct LocalConData {
        shared_ptr<QueuePair> qp;
        shared_ptr<Buffer> buffer;
        shared_ptr<RegionToken> buffer_token;
        shared_ptr<Buffer> tmp_buffer;
        shared_ptr<RegionToken> tmp_buffer_token;
        uint64_t epoch;
        int socket;
    };

   private:
    infinity::core::Context *context;
    QueuePairFactory *qp_factory;
    unordered_map<int, shared_ptr<QueuePair> > existing_qps;  // prevent QPs from being automatically freed
    unique_ptr<NCLMrPool> mr_pool;
    unordered_map<string, LocalConData> local_cons;
    zhandle_t *zh;

    // size_t buf_size;
    // int conn_cnt;
    bool stop;

   public:
    CSLServer(uint16_t port, size_t buf_size, string mgr_hosts="");
    ~CSLServer();

    void Run();
    int GetConnectionCount() { return local_cons.size(); }
    vector<string> GetAllFileId();
    void *GetBufData(const string &fileid) { return local_cons[fileid].buffer->getData(); }
    uint64_t ReadSeqNum(const string &fileid);
    void Stop() { stop = true; }
    void Preload(ifstream &file);

   private:
    size_t findSize(const string &file_id);
    void handleIncomingConnection();
    int handleClientRequest(int socket);

    void finalizeConData(struct LocalConData &con);
};

void ServerWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
