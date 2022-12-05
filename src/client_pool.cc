/*
 * RDMA client pool
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#include "client_pool.h"

#include <glog/logging.h>

#include <algorithm>

using namespace std;

shared_ptr<CSLClient> CSLClientPool::GetClient(set<string> host_address, uint16_t port, size_t buf_size) {
    lock_guard<mutex> guard(lock);

    auto it_cli =
        find_if(idle_clients.begin(), idle_clients.end(), [&](const pair<uint32_t, shared_ptr<CSLClient> > &c) -> bool {
            return (c.second->GetPeers() == host_address) && (c.second->GetBufSize() >= buf_size);
        });

    shared_ptr<CSLClient> cli;
    uint32_t cli_id;
    if (it_cli == idle_clients.end()) {
        cli_id = global_id++;
        cli =
            busy_clients.insert(make_pair(cli_id, make_shared<CSLClient>(host_address, port, buf_size))).first->second;
    } else {
        cli_id = it_cli->first;
        cli = it_cli->second;
        idle_clients.erase(it_cli);
        busy_clients.insert(make_pair(cli_id, cli));
    }
    cli->SetInUse(true);
    return cli;
}

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
