/*
 * RDMA client pool
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

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

    template <typename Client = CSLClient>
    shared_ptr<CSLClient> GetClient(set<string> host_address, uint16_t port, size_t buf_size) {
        lock_guard<mutex> guard(lock);

        auto it_cli = find_if(idle_clients.begin(), idle_clients.end(),
                              [&](const pair<uint32_t, shared_ptr<CSLClient> > &c) -> bool {
                                  return (c.second->GetPeers() == host_address) && (c.second->GetBufSize() >= buf_size);
                              });

        shared_ptr<CSLClient> cli;
        uint32_t cli_id;
        if (it_cli == idle_clients.end()) {
            cli_id = global_id++;
            cli = busy_clients.insert(make_pair(cli_id, make_shared<Client>(host_address, port, buf_size, cli_id)))
                      .first->second;
        } else {
            cli_id = it_cli->first;
            cli = it_cli->second;
            idle_clients.erase(it_cli);
            busy_clients.insert(make_pair(cli_id, cli));
        }
        cli->SetInUse(true);
        return cli;
    }
    void RecycleClient(uint32_t client_id);
    int GetIdleCliCnt() { return idle_clients.size(); }
    int GetBusyCliCnt() { return busy_clients.size(); }
};
