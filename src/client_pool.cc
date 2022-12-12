/*
 * RDMA client pool
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#include "client_pool.h"

#include <glog/logging.h>

using namespace std;

void CSLClientPool::RecycleClient(uint32_t client_id) {
    lock_guard<mutex> guard(lock);

    auto it_cli = busy_clients.find(client_id);
    if (it_cli == busy_clients.end()) {
        LOG(ERROR) << "client " << client_id << " not in busy_clients";
        return;
    }
    uint32_t id = it_cli->first;
    shared_ptr<CSLClient> cli = it_cli->second;
    busy_clients.erase(it_cli);
    cli->Reset();
    idle_clients.insert(make_pair(id, cli));
}
