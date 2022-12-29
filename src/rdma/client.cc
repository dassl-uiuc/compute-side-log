/*
 * Compute-side log RDMA client
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */
#include "client.h"

#include <errno.h>
#include <glog/logging.h>
#include <stdlib.h>

#include <memory>

#include "../csl_config.h"
#include "../util.h"

CSLClient::CSLClient(set<string> host_addresses, uint16_t port, size_t buf_size, uint32_t id, const char *name)
    : buf_size(buf_size), buf_offset(0), in_use(false), id(id), filename(name), zh(nullptr) {
    init(host_addresses, port);
}

CSLClient::CSLClient(uint16_t mgr_port, string mgr_address, size_t buf_size, uint32_t id, const char *name)
    : buf_size(buf_size), buf_offset(0), in_use(false), id(id), filename(name), zh(nullptr) {
    int ret;
    zh = zookeeper_init((mgr_address + ":" + to_string(mgr_port)).c_str(), ClientWatcher, 10000, 0, this, 0);
    if (!zh) {
        LOG(ERROR) << "Failed to init zookeeper handler, errno: " << errno;
        return;
    }

    struct String_vector peerv;
    ret = zoo_get_children(zh, ZK_SVR_ROOT_PATH.c_str(), 0, &peerv);
    if (ret) {
        LOG(ERROR) << "Failed to get servers, errno: " << ret;
        return;
    } else if (peerv.count <= 0) {
        LOG(ERROR) << "No server found";
        return;
    }

    set<string> host_addresses;
    for (int i = 0; i < peerv.count; i++) {
        host_addresses.insert(peerv.data[i]);
    }
    LOG(INFO) << "Found " << peerv.count << " servers: " << generateIpString(host_addresses);

    init(host_addresses, PORT);

    for (auto &p : peers) {
        string peer_path = ZK_SVR_ROOT_PATH + "/" + p;
        struct Stat stat;
        ret = zoo_wexists(zh, peer_path.c_str(), ClientWatcher, this, &stat);
        if (ret) {
            LOG(ERROR) << "Error set watcher on node " << peer_path << " errno:" << ret;
        }
    }

    struct ACL acl[] = {{
        .perms = ZOO_PERM_ALL,
        .id = ZOO_ANYONE_ID_UNSAFE,
    }};
    struct ACL_vector aclv = {
        .count = 1,
        .data = acl,
    };
    string node_path = ZK_CLI_ROOT_PATH + "/" + qp_factory->getIpAddress();
    int value = 0;
    ret = zoo_create(zh, ZK_CLI_ROOT_PATH.c_str(), (const char *)&value, sizeof(value), &aclv, ZOO_PERSISTENT, nullptr,
                     0);
    if (ret && ret != ZNODEEXISTS) {
        LOG(ERROR) << "Failed to create zk node: " << ZK_CLI_ROOT_PATH << ", errno: " << ret;
        return;
    }
    string peers_str = generateIpString(peers);
    ret = zoo_create(zh, node_path.c_str(), peers_str.data(), peers_str.size(), &aclv, ZOO_PERSISTENT, nullptr, 0);
    if (ret) {
        LOG(ERROR) << "Failed to create zk node: " << node_path << ", errno: " << ret;
        return;
    }
}

void CSLClient::init(set<string> host_addresses, uint16_t port) {
    context = new infinity::core::Context(0, 1);
    qp_factory = new infinity::queues::QueuePairFactory(context);

    for (auto &addr : host_addresses) {
        AddPeer(addr, port);
    }

    LOG(INFO) << "Creating buffers";
    if (posix_memalign(&(mem), infinity::core::Configuration::PAGE_SIZE, buf_size)) {
        LOG(FATAL) << "Cannot allocate and align buffer.";
    }
    buffer = new infinity::memory::Buffer(context, mem, buf_size);
    LOG(INFO) << "csl client " << id << " created, buffer size " << buf_size;
}

CSLClient::~CSLClient() {
    string node_path = ZK_CLI_ROOT_PATH + "/" + qp_factory->getIpAddress();
    zoo_delete(zh, node_path.c_str(), -1);
    if (zh) zookeeper_close(zh);
    if (buffer) delete buffer;
    for (auto &p : remote_props) {
        if (p.second.qp) delete p.second.qp;
    }
    if (qp_factory) delete qp_factory;
    if (context) delete context;
}

void CSLClient::ReadSync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    infinity::requests::RequestToken request_token(context);
    RemoteConData &prop = remote_props.begin()->second;
    prop.qp->read(buffer, local_off, prop.remote_buffer_token, remote_off, size, &request_token);
    request_token.waitUntilCompleted();
}

void CSLClient::WriteSync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    vector<shared_ptr<infinity::requests::RequestToken> > request_tokens;
    for (auto &p : remote_props) {
        auto token = make_shared<infinity::requests::RequestToken>(context);
        request_tokens.emplace_back(token);
        p.second.qp->write(buffer, local_off, p.second.remote_buffer_token, remote_off, size, token.get());
    }

    for (auto token : request_tokens) {
        token->waitUntilCompleted();
    }
}

void CSLClient::Append(const void *buf, uint32_t size) {
    size_t cur_off = buf_offset.fetch_add(size);
    memcpy((char *)mem + cur_off, buf, size);
    WriteSync(cur_off, cur_off, size);
}

void CSLClient::Reset() {
    memset(mem, 0, buf_size);
    double usage = buf_offset.load() / 1024.0 / 1024.0;
    buf_offset.store(0);
    SetInUse(false);
    filename.clear();
    LOG(INFO) << "csl client " << id << "recycled, MR usage: " << usage << "MB";
}

bool CSLClient::AddPeer(const string &host_addr, uint16_t port) {
    if (peers.find(host_addr) != peers.end()) {
        LOG(ERROR) << "Peer " << host_addr << " already connected.";
        return false;
    }

    LOG(INFO) << "Connecting to " << host_addr;
    RemoteConData prop;
    struct FileInfo fi;
    fi.size = buf_size;
    strcpy(fi.filename, filename.c_str());
    prop.qp = qp_factory->connectToRemoteHost(host_addr.c_str(), port, &fi, sizeof(fi));
    LOG(INFO) << host_addr << " connected";
    prop.remote_buffer_token = static_cast<infinity::memory::RegionToken *>(prop.qp->getUserData());
    remote_props[host_addr] = prop;
    peers.insert(host_addr);

    return true;
}

bool CSLClient::RemovePeer(string host_addr) {
    auto it = remote_props.find(host_addr);
    if (it == remote_props.end()) {
        LOG(ERROR) << "Peer " << host_addr << " not connected.";
        return false;
    }

    delete it->second.qp;
    remote_props.erase(it);
    peers.erase(host_addr);
    LOG(INFO) << "Client " << id << " removes peer " << host_addr;
    return true;
}

void ClientWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx) {
    CSLClient *cli = reinterpret_cast<CSLClient *>(watcher_ctx);
    LOG(INFO) << "Client watcher triggered on client " << cli->GetId() << ", type: " << type2String(type)
              << " state: " << state2String(state) << " path: " << path;
    if (state == ZOO_CONNECTED_STATE) {
        if (type == ZOO_DELETED_EVENT) {
            string peer;
            stringstream pathss(path);
            // node path: /servers/<peer>
            getline(pathss, peer, '/');
            getline(pathss, peer, '/');
            getline(pathss, peer, '/');
            cli->RemovePeer(peer);
        }
    }
}
