/*
 * Compute-side log RDMA client
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

#include <set>
#include <unordered_map>

using namespace std;

struct FileInfo {
    size_t size;
    char filename[512];
}__attribute__((packed));

class CSLClient {
    friend void ClientWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
    struct RemoteConData {
        infinity::queues::QueuePair *qp;
        infinity::memory::RegionToken *remote_buffer_token;
    };

   protected:
    infinity::core::Context *context;
    infinity::queues::QueuePairFactory *qp_factory;
    unordered_map<string, RemoteConData> remote_props;
    set<string> peers;
    infinity::memory::Buffer *buffer;

    void *mem;
    size_t buf_size;
    atomic<size_t> buf_offset;
    bool in_use;
    uint32_t id;
    string filename;

    zhandle_t *zh;

   public:
    CSLClient() = default;
    CSLClient(set<string> host_addresses, uint16_t port, size_t buf_size, uint32_t id = 0, const char *filename="");
    CSLClient(uint16_t mgr_port, string mgr_address, size_t buf_size, uint32_t id=0, const char *filename="");
    ~CSLClient();

    void WriteSync(uint64_t local_off, uint64_t remote_off, uint32_t size);
    void ReadSync(uint64_t local_off, uint64_t remote_off, uint32_t size);
    void Append(const void *buf, uint32_t size);
    
    void *GetBufData() { return buffer->getData(); }
    virtual void Reset();
    bool AddPeer(const string &host_addr, uint16_t port);
    bool RemovePeer(string host_addr);
    const set<string> &GetPeers() { return peers; }
    size_t GetBufSize() { return buf_size; }
    void SetInUse(bool is_inuse) { in_use = is_inuse; }
    void SetFilename(const char *name) { filename = name; }
    uint32_t GetId() { return id; }

   private:
    void init(set<string> host_addresses, uint16_t port);
};

void ClientWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
