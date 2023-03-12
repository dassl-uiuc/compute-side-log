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

#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>

#include "../csl_config.h"
#include "mr_pool.h"
#include "qp_pool.h"

using namespace std;

class CSLClient {
    friend void ClientWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
    struct RemoteConData {
        shared_ptr<infinity::queues::QueuePair> qp;
        infinity::memory::RegionToken *remote_buffer_token;
        int socket;
    };

   protected:
    infinity::core::Context *context;
    // infinity::queues::QueuePairFactory *qp_factory;
    shared_ptr<NCLQpPool> qp_pool;
    shared_ptr<NCLMrPool> mr_pool;
    unordered_map<string, RemoteConData> remote_props;
    set<string> peers;
    shared_ptr<infinity::memory::Buffer> buffer;

    int rep_factor;
    size_t buf_size;
    atomic<size_t> buf_offset;
    bool in_use;
    uint32_t id;
    string filename;
    mutex recover_lock;

    zhandle_t *zh;

   public:
    CSLClient() = default;
    CSLClient(shared_ptr<NCLQpPool> qp_pool, shared_ptr<NCLMrPool> mr_pool, set<string> host_addresses, size_t buf_size,
              uint32_t id = 0, const char *filename = "");
    CSLClient(shared_ptr<NCLQpPool> qp_pool, shared_ptr<NCLMrPool> mr_pool, string mgr_hosts, size_t buf_size,
              uint32_t id = 0, const char *filename = "", int rep_num = DEFAULT_REP_FACTOR);
    ~CSLClient();

    void WriteSync(uint64_t local_off, uint64_t remote_off, uint32_t size);
    void ReadSync(uint64_t local_off, uint64_t remote_off, uint32_t size);

    /**
     * Append to the end of the log.
     * Behavior of this call is expected to be consistent with glibc WRITE(2)
     *
     * @param buf pointer to the data to be appended
     * @param size size of data to be appended
     */
    ssize_t Append(const void *buf, size_t size);

    /**
     * Read from the log.
     * Behavior of this call is expected to be consistent with glibc READ(2)
     *
     * @param buf pointer to the returned data
     * @param size size of data to be read
     * @return actual size of data read
     */
    ssize_t Read(void *buf, size_t size);

    /**
     * This function does the same as lseek(). See `man lseek` for detail.
     * Not all whence are supported
     */
    off_t Seek(off_t offset, int whence);

    void *GetBufData() { return buffer->getData(); }

    /**
     * Reset the client to unuse state
     */
    virtual void Reset();

    /**
     * Connect to a replication peer
     * @param host_addr address of the peer
     */
    bool AddPeer(const string &host_addr);

    /**
     * Send finalization request to peers to clean state on server
     * @param type type of finalization
     * close file: server will preserve the QP for future use
     * exit process: server will destroy the QP
     */
    void SendFinalization(int type=CLOSE_FILE);

    /**
     * If buffer of a greater size is needed, recycle the buffer and get a new buffer from MR pool
     * @param size size of the new buffer needed
     */
    void ReplaceBuffer(size_t size);

    /**
     * Set file info locally, sync file info with each replication server (fileid, size), and
     * get back MR region token from each replication server. Called at each file openning.
     *
     * @param name name of the (new) file
     * @param size max possible size of the (new) file
     */
    void SetFileInfo(const char *name, size_t size);

    const set<string> &GetPeers() { return peers; }
    size_t GetBufSize() { return buf_size; }
    void SetInUse(bool is_inuse) { in_use = is_inuse; }
    uint32_t GetId() { return id; }

   private:
    void init(set<string> host_addresses);
    void createClientZKNode();

    /**
     * @return number of peers already exists for this client. If this is the first time the client
     * connects to ZK, 0 will be returned
     */
    int getPeersFromZK(set<string> &peer_ips);
    /**
     * Remove a replication peer from the client and add a new replication peer
     * @param old_addr address of the peer to be removed
     * @return address of the new peer, empty if replacement failed
     */
    string replacePeer(string &old_addr);

    /**
     * Bring the state of a new peer up to date
     * @param new_addr address of the peer to be recovered
     * @return true if recovery is successful
     */
    bool recoverPeer(string &new_addr);

    /**
     * Get the ip address of the replication server from which the client recover the lost data.
     * Usually called after an client crash.
     * @return ip address of replication server that serves as recover source and number of bytes to recover
     */
    tuple<string, size_t> getRecoverSrcPeer();

    /**
     * Get the lost data from a replication server
     * 
     * @param recover_src ip of the replication server to get the data from
     * @param size size to get from the replication server
    */
    void recoverFromSrc(string &recover_src, size_t size);

    /**
     * A human-readable unique identifier of each file
     */
    const string getFileIdentifier() {
        return QueuePairFactory::getIpAddress() + ":" + filename;  // e.g. "10.0.0.1:/home/user/001.log"
    }

    /**
     * An unique identifier of each file for using in ZK (since ZK node name can't contain '/')
     */
    const string getZkNodeName() {
        return QueuePairFactory::getIpAddress() + ":" + to_string(hash<string>()(filename));  // e.g. "10.0.0.1:1234567890"
    }
};

void ClientWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx);
