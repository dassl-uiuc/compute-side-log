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

#include <unordered_map>

#include <infinity/core/Configuration.h>
#include <infinity/core/Context.h>
#include <infinity/memory/Buffer.h>
#include <infinity/memory/RegionToken.h>
#include <infinity/queues/QueuePair.h>
#include <infinity/queues/QueuePairFactory.h>
#include <zookeeper/zookeeper.h>

#include "../csl_config.h"

using namespace std;

class CSLServer {
    friend void ServerWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
    struct LocalConData {
        infinity::queues::QueuePair *qp;
        infinity::memory::Buffer *buffer;
        infinity::memory::RegionToken *buffer_token;
        int socket;
    };

   private:
    infinity::core::Context *context;
    infinity::queues::QueuePairFactory *qp_factory;
    unordered_map<string, LocalConData> local_cons;
    zhandle_t *zh;

    size_t buf_size;
    // int conn_cnt;
    bool stop;

   public:
    CSLServer(uint16_t port, size_t buf_size, string mgr_hosts="");
    ~CSLServer();

    void Run();
    int GetConnectionCount() { return local_cons.size(); }
    vector<string> GetAllFileId();
    void *GetBufData(const string &fileid) { return local_cons[fileid].buffer->getData(); }
    void Stop() { stop = true; }

   private:
    size_t findSize(const string &file_id);
    void handleIncomingConnection();
    int handleClientRequest(int socket);

    void finalizeConData(struct LocalConData &con);
};

void ServerWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
