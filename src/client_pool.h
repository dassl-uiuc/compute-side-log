/*
 * RDMA client pool
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#pragma once

#include <infinity/core/Context.h>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

#include "csl_config.h"
#include "rdma/client.h"
#include "rdma/qp_pool.h"

using namespace std;

class CSLClientPool {
   private:
    map<uint32_t, shared_ptr<CSLClient> > idle_clients;
    map<uint32_t, shared_ptr<CSLClient> > busy_clients;
    uint32_t global_id;
    const string mgr_hosts;

    Context *context;
    shared_ptr<NCLQpPool> qp_pool;
    shared_ptr<NCLMrPool> mr_pool;

    mutex lock;

   public:
    CSLClientPool(string mgr_hosts = ZK_DEFAULT_HOST);

    // Deprecated
    shared_ptr<CSLClient> GetClient(set<string> host_address, size_t buf_size, const char *filename = "");

    /**
     * Get a client used for file replication
     * 
     * @param buf_size size of memory required to back up this file
     * @param filename name of the file
     * @param try_recover if true, the client will try to consult the peers to see if the file has been replicated there
     * and can be recovered. if false, the file will be initialized as empty
    */
    shared_ptr<CSLClient> GetClient(size_t buf_size, const char *filename, bool try_recover=false);

    void RecycleClient(uint32_t client_id);
    int GetIdleCliCnt() { return idle_clients.size(); }
    int GetBusyCliCnt() { return busy_clients.size(); }
};
