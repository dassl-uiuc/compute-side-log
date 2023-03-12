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

#include <chrono>
#include <memory>

#include "../csl_config.h"
#include "../util.h"
#include "common.h"

#define FORCE_REMOTE_READ 1

using infinity::queues::QueuePairFactory;

CSLClient::CSLClient(shared_ptr<NCLQpPool> qp_pool, shared_ptr<NCLMrPool> mr_pool, set<string> host_addresses,
                     size_t buf_size, uint32_t id, const char *name)
    : qp_pool(qp_pool),
      mr_pool(mr_pool),
      rep_factor(host_addresses.size()),
      buf_size(buf_size),
      buf_offset(0),
      in_use(false),
      id(id),
      filename(name),
      zh(nullptr) {
    init(host_addresses);
}

CSLClient::CSLClient(shared_ptr<NCLQpPool> qp_pool, shared_ptr<NCLMrPool> mr_pool, string mgr_hosts, size_t buf_size,
                     uint32_t id, const char *name, int rep_num, bool try_recover)
    : qp_pool(qp_pool),
      mr_pool(mr_pool),
      rep_factor(rep_num),
      buf_size(buf_size),
      buf_offset(0),
      in_use(false),
      id(id),
      filename(name),
      zh(nullptr) {
    int ret, n_peers;
    zh = zookeeper_init(mgr_hosts.c_str(), ClientWatcher, 10000, 0, this, 0);
    if (!zh) {
        LOG(ERROR) << "Failed to init zookeeper handler, errno: " << errno;
        return;
    }

    set<string> host_addresses;
    n_peers = getPeersFromZK(host_addresses);  // check if client node has been created before

    init(host_addresses);

    for (auto &p : peers) {
        string peer_path = ZK_SVR_ROOT_PATH + "/" + p;
        struct Stat stat;
        ret = zoo_wexists(zh, peer_path.c_str(), ClientWatcher, this, &stat);
        if (ret) {
            LOG(ERROR) << "Error set watcher on node " << peer_path << " errno:" << ret;
        }
    }

    if (n_peers == 0) {
        createClientZKNode();  // node doesn't exist, need to create client ZK node
    } else if (try_recover){
        TryRecover();
    }
}

void CSLClient::init(set<string> host_addresses) {
    //  context and qp_factory construction moved outside
    context = qp_pool->GetContext();

    for (auto &addr : host_addresses) {
        AddPeer(addr);
    }

    LOG(INFO) << "Creating buffers";
    buffer = mr_pool->GetMRofSize(buf_size);
    LOG(INFO) << "csl client " << id << " created, buffer size " << buf_size;
}

int CSLClient::getPeersFromZK(set<string> &peer_ips) {
    int ret, buf_len = 512;
    string node_path = ZK_CLI_ROOT_PATH + "/" + getZkNodeName();
    char peers_buf[512];
    ret = zoo_get(zh, node_path.c_str(), 0, peers_buf, &buf_len, nullptr);
    if (ret) {  // client node doesn't exist, get peer ip from /servers
        if (ret != ZNONODE) {
            LOG(ERROR) << "Failed to get zk node " << node_path << ", errno: " << ret;
            return 0;
        } else {
            struct String_vector peerv;
            ret = zoo_get_children(zh, ZK_SVR_ROOT_PATH.c_str(), 0, &peerv);
            if (ret) {
                LOG(ERROR) << "Failed to get servers, errno: " << ret;
                return 0;
            } else if (peerv.count <= 0) {
                LOG(ERROR) << "No server found";
                return 0;
            } else if (peerv.count < rep_factor) {
                LOG(WARNING) << "Insufficient replication servers, require " << rep_factor << ", found " << peerv.count;
                rep_factor = peerv.count;  // working at reduced reliability
            }

            for (int i = 0; i < rep_factor; i++) {
                peer_ips.insert(peerv.data[i]);
            }
            LOG(INFO) << "Found " << peerv.count << " servers: " << generateIpString(peer_ips);
            return 0;
        }
    } else {  // client node exist
        string peers_str(peers_buf, buf_len);
        LOG(INFO) << "Reconnect to servers: " << peers_str;
        return parseIpString(peers_str, peer_ips);
    }
}

void CSLClient::createClientZKNode() {
    int ret;
    struct ACL acl[] = {{
        .perms = ZOO_PERM_ALL,
        .id = ZOO_ANYONE_ID_UNSAFE,
    }};
    struct ACL_vector aclv = {
        .count = 1,
        .data = acl,
    };
    string node_path = ZK_CLI_ROOT_PATH + "/" + getZkNodeName();
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

CSLClient::~CSLClient() {
    int ret;
    if (in_use) {
        SendFinalization(EXIT_PROC);  // destroy QP on server side
    }
    if (zh) {
        string node_path = ZK_CLI_ROOT_PATH + "/" + getZkNodeName();
        ret = zoo_delete(zh, node_path.c_str(), -1);
        if (ret) {
            LOG(ERROR) << "Failed to delete znode: " << node_path << ", errorno: " << ret;
        }
        zookeeper_close(zh);
    }
    if (buffer) mr_pool->RecycleMR(buffer);
    for (auto &p : remote_props) {
        qp_pool->RecycleQp(p.second.qp);
    }
}

void CSLClient::ReadSync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    infinity::requests::RequestToken request_token(context);
    RemoteConData &prop = remote_props.begin()->second;
    prop.qp->read(buffer.get(), local_off, prop.remote_buffer_token, remote_off, size, &request_token);
    request_token.waitUntilCompleted();
}

void CSLClient::WriteSync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    vector<shared_ptr<infinity::requests::RequestToken> > request_tokens;
    for (auto &p : remote_props) {
        auto token = make_shared<infinity::requests::RequestToken>(context);
        request_tokens.emplace_back(token);
        p.second.qp->write(buffer.get(), local_off, p.second.remote_buffer_token, remote_off, size, token.get());
    }

    for (auto token : request_tokens) {
        token->waitUntilCompleted();
    }
}

ssize_t CSLClient::Append(const void *buf, size_t size) {
    // disable new append during recovering
    lock_guard<mutex> guard(recover_lock);  // TODO: enable async recovery
    size = min(size, buffer->getSizeInBytes() - buf_offset);
    size_t cur_off = buf_offset.fetch_add(size);
    memcpy((char *)buffer->getAddress() + cur_off, buf, size);
    WriteSync(cur_off, cur_off, size);
    return size;
}

ssize_t CSLClient::Read(void *buf, size_t size) {
    // TODO: read from remote MR?
    size = min(size, buffer->getSizeInBytes() - buf_offset);
    size_t cur_off = buf_offset.fetch_add(size);
#if FORCE_REMOTE_READ
    ReadSync(cur_off, cur_off, size);
#endif
    memcpy(buf, (char *)buffer->getAddress() + cur_off, size);
    return size;
}

off_t CSLClient::Seek(off_t offset, int whence) {
    switch (whence) {
        case SEEK_SET:
            buf_offset = min((size_t)offset, buffer->getSizeInBytes() - 1);
            break;
        case SEEK_CUR:
            buf_offset = min((size_t)(offset + buf_offset), buffer->getSizeInBytes() - 1);
            break;
        case SEEK_END:
            // TODO: need to know the current size of file (not buffer size)
            break;
        default:
            LOG(WARNING) << "whence " << whence << " is not supported";
    };
    return buf_offset.load();
}

void CSLClient::Reset() {
    memset((void *)buffer->getAddress(), 0, buf_size);
    double usage = buf_offset.load() / 1024.0 / 1024.0;
    buf_offset.store(0);
    SendFinalization();
    SetInUse(false);
    filename.clear();
    LOG(INFO) << "csl client " << id << " recycled, MR usage: " << usage << "MB";
}

bool CSLClient::AddPeer(const string &host_addr) {
    if (peers.find(host_addr) != peers.end()) {
        LOG(ERROR) << "Peer " << host_addr << " already connected.";
        return false;
    }

    LOG(INFO) << "Connecting to " << host_addr;
    RemoteConData prop;
    struct FileInfo fi;
    fi.size = buf_size;
    const string file_identifier = getFileIdentifier();
    strcpy(fi.file_id, file_identifier.c_str());
    prop.qp = qp_pool->GetQpTo(host_addr, &fi);
    prop.socket = prop.qp->getRemoteSocket();
    LOG(INFO) << host_addr << " connected";
    prop.remote_buffer_token = static_cast<infinity::memory::RegionToken *>(prop.qp->getUserData());
    remote_props[host_addr] = prop;
    peers.insert(host_addr);

    return true;
}

string CSLClient::replacePeer(string &old_addr) {
    string new_addr;
    struct String_vector peerv;
    int ret, i;

    // remove old peer
    auto it = remote_props.find(old_addr);
    if (it == remote_props.end()) {
        LOG(ERROR) << "Peer " << old_addr << " not connected.";
        return "";
    }

    // qp_pool->RecycleQp(it->second.qp);
    // drop qp and not recycle since it's disconnected ? can we recycle it?
    remote_props.erase(it);
    peers.erase(old_addr);
    LOG(INFO) << "Client " << id << " removes peer " << old_addr;

    // find a new peer from ZK
    ret = zoo_get_children(zh, ZK_SVR_ROOT_PATH.c_str(), 0, &peerv);
    if (ret) {
        LOG(ERROR) << "Failed to get servers, errno: " << ret;
        return "";
    } else if (peerv.count <= 0) {
        LOG(ERROR) << "No server found";
        return "";
    }

    for (i = 0; i < peerv.count; i++) {
        // For now we don't allow replace with the same peer even though it comes back
        if ((peers.find(peerv.data[i]) == peers.end()) && (old_addr.compare(peerv.data[i]) == 0)) {
            new_addr = peerv.data[i];  // find a new peer different from current peers
            break;
        }
    }
    if (i >= peerv.count) {
        LOG(ERROR) << "Failed to find new peers";
        return "";
    }

    if (AddPeer(new_addr)) {
        LOG(INFO) << "Replaced old peer " << old_addr << " with new peer " << new_addr;
        return new_addr;
    } else {
        return "";
    }
}

bool CSLClient::recoverPeer(string &new_peer) {
    lock_guard<mutex> guard(recover_lock);
    auto token = make_shared<infinity::requests::RequestToken>(context);
    auto &p = remote_props[new_peer];
    p.qp->write(buffer.get(), p.remote_buffer_token, buf_offset.load(), token.get());  // ? need mannual segmentation?
    token->waitUntilCompleted();
    return true;
}

tuple<string, size_t> CSLClient::getRecoverSrcPeer() {
    const string file_id = getFileIdentifier();
    size_t min_size = UINT64_MAX;
    string ip_recover_src;
    for (auto &p : remote_props) {
        struct ClientReq getinfo_req;
        size_t size = 0;

        getinfo_req.type = GET_INFO;
        getinfo_req.fi.size = 0;
        strcpy(getinfo_req.fi.file_id, file_id.c_str());
        send(p.second.socket, &getinfo_req, sizeof(getinfo_req), 0);
        recv(p.second.socket, &size, sizeof(size), 0);
        if (size != 0 && size < min_size) {
            min_size = size;
            ip_recover_src = p.first;
        }
    }
    if (min_size == UINT64_MAX)
        min_size = 0;  // none server has replication for this file 
    return make_tuple(ip_recover_src, min_size);
}

void CSLClient::recoverFromSrc(string &recover_src, size_t size) {
    auto &p = remote_props[recover_src];
    auto token = make_shared<infinity::requests::RequestToken>(context);
    p.qp->read(buffer.get(), p.remote_buffer_token, size, token.get());  // ? need mannual segmentation?
    token->waitUntilCompleted();
    buf_offset = size;
}

void CSLClient::SendFinalization(int type) {
    if (!in_use) return;
    ClientReq req;
    const string file_identifier = getFileIdentifier();
    strcpy(req.fi.file_id, file_identifier.c_str());
    req.type = type;

    for (auto &p : remote_props) {
        send(p.second.socket, &req, sizeof(req), 0);
    }
}

void CSLClient::ReplaceBuffer(size_t size) {
    if (size <= buffer->getSizeInBytes()) return;
    mr_pool->RecycleMR(buffer);
    buffer = mr_pool->GetMRofSize(size);
}

void CSLClient::SetFileInfo(const char *name, size_t size) {
    filename = name;
    buf_size = size;
    const string file_identifier = getFileIdentifier();
    ClientReq open_req;
    open_req.type = OPEN_FILE;
    open_req.fi.size = size;
    strcpy(open_req.fi.file_id, file_identifier.c_str());
    for (auto &c : remote_props) {
        send(c.second.qp->getRemoteSocket(), &open_req, sizeof(open_req), 0);
    }
    for (auto &c : remote_props) {
        recv(c.second.qp->getRemoteSocket(), c.second.qp->getUserData(), sizeof(RegionToken), 0);
    }
}

void CSLClient::TryRecover() {
    // todo: get file info on creating connection to save 1 rtt
    string recover_src;
    size_t recover_size;
    std::tie(recover_src, recover_size) = getRecoverSrcPeer();

    if (recover_size > 0) {
        LOG(INFO) << "recover " << recover_size << "B from " << recover_src;
        recoverFromSrc(recover_src, recover_size);
    }
}

void ClientWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx) {
    CSLClient *cli = reinterpret_cast<CSLClient *>(watcher_ctx);
    LOG(INFO) << "Client watcher triggered on client " << cli->GetId() << ", type: " << type2String(type)
              << " state: " << state2String(state) << " path: " << path;
    if (state == ZOO_CONNECTED_STATE) {
        if (type == ZOO_DELETED_EVENT) {
            const auto start = chrono::high_resolution_clock::now();

            cli->rep_factor -= 1;
            string peer, new_peer;
            stringstream pathss(path);
            // node path: /servers/<peer>
            getline(pathss, peer, '/');
            getline(pathss, peer, '/');
            getline(pathss, peer, '/');
            if ((new_peer = cli->replacePeer(peer)).empty()) {
                LOG(ERROR) << "replacement failed";
                return;
            }
            if (cli->recoverPeer(new_peer)) cli->rep_factor += 1;

            const auto end = chrono::high_resolution_clock::now();
            LOG(INFO) << "Replace and recover a peer takes "
                      << chrono::duration_cast<chrono::microseconds>(end - start).count() << "us";
        }
    }
}
