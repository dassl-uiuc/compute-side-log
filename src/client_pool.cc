/*
 * RDMA client pool
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#include "client_pool.h"

#include <glog/logging.h>

using namespace std;

CSLClientPool::CSLClientPool(string mgr_hosts) : global_id(0), mgr_hosts(mgr_hosts) {
    context = new infinity::core::Context(infinity::core::Configuration::DEFAULT_IB_DEVICE, 1);
    qp_pool = make_shared<NCLQpPool>(context, PORT);
    mr_pool = make_shared<NCLMrPool>(context);
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

shared_ptr<CSLClient> CSLClientPool::GetClient(set<string> host_address, size_t buf_size, const char *filename) {
    lock_guard<mutex> guard(lock);

    shared_ptr<CSLClient> cli;
    uint32_t cli_id;
    if (idle_clients.empty()) {
        cli_id = global_id++;
        auto cli = busy_clients.insert(
            make_pair(cli_id, make_shared<CSLClient>(qp_pool, mr_pool, host_address, buf_size, cli_id, filename)));
    } else {
        auto it = idle_clients.begin();
        cli_id = it->first;
        cli = it->second;
        // TODO: replace peers if needed
        cli->ReplaceBuffer(buf_size);
        cli->SetFileInfo(filename, buf_size);
        idle_clients.erase(it);
        busy_clients.insert(make_pair(cli_id, cli));
    }

    cli->SetInUse(true);
    return cli;
}

shared_ptr<CSLClient> CSLClientPool::GetClient(size_t buf_size, const char *filename, bool try_recover) {
    lock_guard<mutex> guard(lock);

    shared_ptr<CSLClient> cli;
    uint32_t cli_id;
    if (idle_clients.empty()) {
        cli_id = global_id++;
        cli = busy_clients
                  .insert(make_pair(cli_id, make_shared<CSLClient>(qp_pool, mr_pool, mgr_hosts, buf_size, cli_id,
                                                                   filename, DEFAULT_REP_FACTOR, try_recover)))
                  .first->second;
    } else {
        auto it = idle_clients.begin();
        cli_id = it->first;
        cli = it->second;
        cli->ReplaceBuffer(buf_size);
        cli->SetFileInfo(filename, buf_size);
        if (try_recover)
            cli->TryRecover();
        idle_clients.erase(it);
        busy_clients.insert(make_pair(cli_id, cli));
    }
    cli->SetInUse(true);
    return cli;
}
