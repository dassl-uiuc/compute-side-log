/*
 * RDMA client pool
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#pragma once

#include <memory>
#include <set>
#include <string>
#include <map>
#include <mutex>

#include "rdma/client.h"

using namespace std;

class CSLClientPool {
   private:
    map<uint32_t, shared_ptr<CSLClient> > idle_clients;
    map<uint32_t, shared_ptr<CSLClient> > busy_clients;
    uint32_t global_id;

    mutex lock;

   public:
    CSLClientPool() : global_id(0) {}

    shared_ptr<CSLClient> GetClient(set<string> host_addresses, uint16_t port, size_t buf_size);
    void RecycleClient(uint32_t client_id);
};
