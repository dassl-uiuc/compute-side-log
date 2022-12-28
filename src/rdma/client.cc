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
#include <sstream>

#include "../csl_config.h"
#include "../util.h"

CSLClient::CSLClient(set<string> host_addresses, uint16_t port, size_t buf_size, uint32_t id)
    : buf_size(buf_size), buf_offset(0), in_use(false), id(id), zh(nullptr) {
    init(host_addresses, port);
}

CSLClient::CSLClient(uint16_t mgr_port, string mgr_address, size_t buf_size, uint32_t id)
    : buf_size(buf_size), buf_offset(0), in_use(false), id(id), zh(nullptr) {
    int ret;
    zh = zookeeper_init((mgr_address + ":" + to_string(mgr_port)).c_str(), ClientWatcher, 10000, 0, this, 0);
    if (!zh) {
        LOG(ERROR) << "Failed to init zookeeper handler, errno: " << errno;
        return;
    }

    struct String_vector peerv;
    ret = zoo_get_children(zh, ZK_ROOT_PATH.c_str(), 0, &peerv);
    if (ret) {
        LOG(ERROR) << "Failed to get servers, errno: " << ret;
        return;
    } else if (peerv.count <= 0) {
        LOG(ERROR) << "No server found";
        return;
    }

    stringstream peerss;
    set<string> host_addresses;
    for (int i = 0; i < peerv.count; i++) {
        host_addresses.insert(peerv.data[i]);
        peerss << " " << peerv.data[i];
    }
    LOG(INFO) << "Found " << peerv.count << " servers:" << peerss.str();

    init(host_addresses, PORT);

    for (auto &p : peers) {
        string peer_path = ZK_ROOT_PATH + "/" + p;
        struct Stat stat;
        ret = zoo_wexists(zh, peer_path.c_str(), ClientWatcher, this, &stat);
        if (ret) {
            LOG(ERROR) << "Error set watcher on node " << peer_path << " errno:" << ret;
        }
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
    if (zh)
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
    LOG(INFO) << "csl client " << id << "recycled, MR usage: " << usage << "MB";
}

bool CSLClient::AddPeer(const string &host_addr, uint16_t port) {
    if (peers.find(host_addr) != peers.end()) {
        LOG(ERROR) << "Peer " << host_addr << " already connected.";
        return false;
    }

    LOG(INFO) << "Connecting to " << host_addr;
    RemoteConData prop;
    prop.qp = qp_factory->connectToRemoteHost(host_addr.c_str(), port, &buf_size, sizeof(buf_size));
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
