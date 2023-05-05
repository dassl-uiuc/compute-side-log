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
#include <sys/stat.h>

#include <chrono>
#include <memory>

#include "../csl_config.h"
#include "../util.h"
#include "common.h"

#define FORCE_REMOTE_READ   0
#define USE_QUORUM_WRITE    1

using infinity::queues::QueuePairFactory;

CSLClient::CSLClient(shared_ptr<NCLQpPool> qp_pool, shared_ptr<NCLMrPool> mr_pool, set<string> host_addresses,
                     size_t buf_size, uint32_t id, const char *name)
    : qp_pool(qp_pool),
      mr_pool(mr_pool),
      run(true),
      rep_factor(host_addresses.size()),
      buf_size(buf_size),
      buf_offset(0),
      file_size(0),
      seq(0),
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
      run(true),
      rep_factor(rep_num),
      buf_size(buf_size),
      buf_offset(0),
      file_size(0),
      seq(0),
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

    if (n_peers == 0) {
        createClientZKNode();  // node doesn't exist, need to create client ZK node
    } else if (try_recover){
        TryRecover();
    }
#if USE_QUORUM_WRITE && ASYNC_QUORUM_POLL
    cq_poll_th = thread(&CSLClient::CQPollingFunc, this);
#endif
}

void CSLClient::init(set<string> host_addresses) {
    //  context and qp_factory construction moved outside
    context = qp_pool->GetContext();

    for (auto &addr : host_addresses) {
        AddPeer(addr);
    }

    LOG(INFO) << "Creating buffers";
    buffer = mr_pool->GetMRofSize(buf_size);
    seq_offset = buf_size - sizeof(uint64_t);
    seq_addr = reinterpret_cast<uint64_t *>(buffer->getAddressWithOffset(seq_offset));
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
    int ret = 0;

#if USE_QUORUM_WRITE && ASYNC_QUORUM_POLL
    run = false;
    cq_poll_th.join();
#endif

    if (in_use) {
        SendFinalization(EXIT_PROC);  // destroy QP on server side
    }
    if (zh) {
        string node_path = ZK_CLI_ROOT_PATH + "/" + getZkNodeName();
        // ret = zoo_delete(zh, node_path.c_str(), -1);
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
    RequestToken request_token(context);
    RemoteConData &prop = remote_props.begin()->second;
    prop.qp->read(buffer.get(), local_off, prop.remote_buffer_token, remote_off, size, &request_token);
    request_token.waitUntilCompleted();
}

void CSLClient::WriteSync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    vector<shared_ptr<CombinedRequestToken> > combined_req_tokens;
    uint64_t local_offs[2] = {local_off, seq_offset};
    uint64_t remote_offs[2] = {remote_off, seq_offset};
    uint32_t sizes[2] = {size, sizeof(uint64_t)};

    for (auto &p : remote_props) {
        auto token = make_shared<CombinedRequestToken>(context);
        combined_req_tokens.emplace_back(token);
        RequestToken *tokens[2] = {&token->data_token_, &token->seq_token_};
        p.second.qp->writeTwoPlace(buffer.get(), local_offs, p.second.remote_buffer_token, remote_offs, sizes, tokens);
    }

    for (auto token : combined_req_tokens) {
        token->WaitUntilBothCompleted();
    }
}

void CSLClient::WriteQuorum(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    lock_guard<mutex> guard(recover_lock);

    vector<shared_ptr<CombinedRequestToken> > request_tokens;
    uint64_t local_offs[2] = {local_off, seq_offset};
    uint64_t remote_offs[2] = {remote_off, seq_offset};
    uint32_t sizes[2] = {size, sizeof(uint64_t)};
    
    for (auto &p : remote_props) {
        auto token = make_shared<CombinedRequestToken>(context);
        request_tokens.emplace_back(token);
        {
#if ASYNC_QUORUM_POLL
            lock_guard<mutex> lk(poll_lock);
#endif
            p.second.op_queue.push(token);
        }
        RequestToken *tokens[2] = {&token->data_token_, &token->seq_token_};
        p.second.qp->writeTwoPlace(buffer.get(), local_offs, p.second.remote_buffer_token, remote_offs, sizes, tokens);
    }

    do {
#if ASYNC_QUORUM_POLL
#else
        // todo: what if one rep fail-slow? (queue will build up)
        for (auto &p : remote_props) {
            auto &op_q = p.second.op_queue;
            if (op_q.empty())
                continue;
            if (op_q.front()->CheckIfBothCompleted()) {  // will poll CQ once if not completed
                op_q.front()->SetAllPrevCompleted();
                op_q.pop();
            }
        }
#endif
    } while (!quorumCompleted(request_tokens));
}

bool CSLClient::quorumCompleted(vector<shared_ptr<CombinedRequestToken>> &tokens) {
    uint n = 0;
    for (auto &t : tokens) {
        if (t->CheckAllPrevCompleted()) n++;
        if (n > rep_factor / 2) return true;
    }
    return false;
}

void CSLClient::CQPollingFunc() {
    LOG(INFO) << "CQ Polling Thread running";
    while (run) {
        for (auto &p : remote_props) {
            auto &op_q = p.second.op_queue;
            if (op_q.empty())
                continue;
            {
#if ASYNC_QUORUM_POLL
                lock_guard<mutex> lk(poll_lock);
#endif
                if (op_q.front()->CheckIfBothCompleted()) {  // will poll CQ once if not completed
                    op_q.front()->SetAllPrevCompleted();
                    op_q.pop();
                }
            }
        }
    }
    LOG(INFO) << "CQ Polling Thread exit";
}

ssize_t CSLClient::Append(const void *buf, size_t size) {
    size = min(size, buffer->getSizeInBytes() - buf_offset);
    size_t cur_off = buf_offset.fetch_add(size);
    file_size = max(file_size, buf_offset.load());
    memcpy((char *)buffer->getAddress() + cur_off, buf, size);
    *seq_addr = seq.fetch_add(1);
#if USE_QUORUM_WRITE
    WriteQuorum(cur_off, cur_off, size);
#else
    WriteSync(cur_off, cur_off, size);
#endif
    return size;
}

ssize_t CSLClient::WritePos(const void *buf, size_t size, off_t pos) {
    size = min(size, buffer->getSizeInBytes() - pos);
    file_size = max(pos + size, file_size);
    memcpy((char *)buffer->getAddress() + pos, buf, size);
    *seq_addr = seq.fetch_add(1);
#if USE_QUORUM_WRITE
    WriteQuorum(pos, pos, size);
#else
    WriteSync(pos, pos, size);
#endif
    return size;
}

ssize_t CSLClient::Read(void *buf, size_t size) {
    size = min(size, file_size - buf_offset);
    size_t cur_off = buf_offset.fetch_add(size);
#if FORCE_REMOTE_READ
    ReadSync(cur_off, cur_off, size);
#endif
    memcpy(buf, (char *)buffer->getAddress() + cur_off, size);
    return size;
}

ssize_t CSLClient::ReadPos(void *buf, size_t size, off_t pos) {
    size = min(size, file_size - pos);
#if FORCE_REMOTE_READ
    ReadSync(pos, pos, size);
#endif
    memcpy(buf, (char *)buffer->getAddress() + pos, size);
    return size;
}

off_t CSLClient::Seek(off_t offset, int whence) {
    switch (whence) {
        case SEEK_SET:
            buf_offset.store(min((size_t)offset, buffer->getSizeInBytes() - 1));
            break;
        case SEEK_CUR:
            buf_offset.store(min((size_t)(offset + buf_offset), buffer->getSizeInBytes() - 1));
            break;
        case SEEK_END:
            buf_offset.store(file_size);
            break;
        default:
            LOG(WARNING) << "whence " << whence << " is not supported";
    };
    return buf_offset.load();
}

int CSLClient::Truncate(off_t length) {
    // todo: currently we do not maintain the size so no action is taken to adjust the size
    file_size = length;
    LOG(INFO) << "current size " << buf_offset << " truncate to " << length;
    if (length < buf_offset) {
        memset((char*)buffer->getData() + length, 0, buf_offset.exchange(length) - length);
    }
    return 0;
    
}

char *CSLClient::GetLine(char *s, int size) {
    if (Eof())
        return nullptr;

    char *p = reinterpret_cast<char *>(buffer->getAddress() + buf_offset);
    size_t cur_off = buf_offset.load();
    int len = 0;
    while (len < size - 1) {
        ++len;
        if (*(p + len) == '\n') {
            ++len;
            break;
        } else if (cur_off + len >= file_size)
            break;
    }
    memcpy((void *)s, p, len);
    *reinterpret_cast<char *>(s + len) = '\0';
    buf_offset.fetch_add(len);
    return s;
}

void CSLClient::Reset() {
    memset((void *)buffer->getAddress(), 0, buf_size);
    double usage = buf_offset.load() / 1024.0 / 1024.0;
    buf_offset.store(0);
    SendFinalization(CLOSE_FILE);
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
    RemoteConData &prop = remote_props.insert(make_pair(host_addr, RemoteConData())).first->second;
    struct FileInfo fi;
    fi.size = buf_size;
    const string file_identifier = getFileIdentifier();
    strcpy(fi.file_id, file_identifier.c_str());
    prop.qp = qp_pool->GetQpTo(host_addr, &fi);
    prop.socket = prop.qp->getRemoteSocket();
    LOG(INFO) << host_addr << " connected";
    prop.remote_buffer_token = static_cast<infinity::memory::RegionToken *>(prop.qp->getUserData());
    peers.insert(host_addr);

    string peer_path = ZK_SVR_ROOT_PATH + "/" + host_addr;
    struct Stat stat;
    int ret = zoo_wexists(zh, peer_path.c_str(), ClientWatcher, this, &stat);
    if (ret) {
        LOG(ERROR) << "Error set watcher on node " << peer_path << " errno:" << ret;
        return false;
    }

    return true;
}

string CSLClient::replacePeer(string &old_addr) {
    string new_addr;
    struct String_vector peerv;
    int ret, i;

    if (!old_addr.empty()) {
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
    }

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
        if (peers.find(peerv.data[i]) == peers.end()) {
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

bool CSLClient::recoverPeer(const string &new_peer) {
    auto token = make_shared<CombinedRequestToken>(context);
    auto &p = remote_props[new_peer];

    uint64_t local_offs[2] = {0, seq_offset};
    uint64_t remote_offs[2] = {0, seq_offset};
    uint32_t sizes[2] = {static_cast<uint32_t>(file_size), sizeof(uint64_t)};  // todo: if size > max_uint32, need multiple writes
    RequestToken *tokens[2] = {&token->data_token_, &token->seq_token_};
    p.qp->writeTwoPlace(buffer.get(), local_offs, p.remote_buffer_token, remote_offs, sizes, tokens);
    token->WaitUntilBothCompleted();
    return true;
}

tuple<string, size_t> CSLClient::getRecoverSrcPeer() {
    const string file_id = getFileIdentifier();
    uint64_t min_seq = UINT64_MAX;
    size_t recover_size;
    string ip_recover_src;
    for (auto &p : remote_props) {
        struct ClientReq getinfo_req;
        getinfo_req.type = GET_INFO;
        getinfo_req.fi.size = 0;
        strcpy(getinfo_req.fi.file_id, file_id.c_str());
        send(p.second.socket, &getinfo_req, sizeof(getinfo_req), 0);
    }

    for (auto &p : remote_props) {
        struct ServerResp getinfo_resp;
        recv(p.second.socket, &getinfo_resp, sizeof(getinfo_resp), 0);
        if (getinfo_resp.seq != 0 && getinfo_resp.seq < min_seq) {
            min_seq = getinfo_resp.seq;
            recover_size = getinfo_resp.size;
            ip_recover_src = p.first;
        }
    }
    if (min_seq == UINT64_MAX)
        recover_size = 0;  // none server has replication for this file 
    return make_tuple(ip_recover_src, recover_size);
}

void CSLClient::recoverFromSrc(string &recover_src, size_t size) {
    auto &p = remote_props[recover_src];
    auto token = make_shared<RequestToken>(context);
    p.qp->read(buffer.get(), p.remote_buffer_token, size, token.get());  // todo: if size > max_uint32, need multiple writes
    token->waitUntilCompleted();
    file_size = size;
}

void CSLClient::syncPeerAfterRecover(const string &skip_peer) {
    for (auto &p : peers) {
        if (p.compare(skip_peer) == 0) continue;
        recoverPeer(p);
    }
}

void CSLClient::watchForPeerJoin() {
    int ret = zoo_get_children(zh, ZK_SVR_ROOT_PATH.c_str(), 1, nullptr);
    if (ret) {
        LOG(ERROR) << "Failed to watch on servers/, errno: " << ret;
    }
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
        LOG(INFO) << "recover " << recover_size << "B for " << filename << " from " << recover_src;
        recoverFromSrc(recover_src, recover_size);
    }

    syncPeerAfterRecover(recover_src);
}

void CSLClient::TryLocalRecover(int fd) {
    struct stat log_stat;
    fstat(fd, &log_stat);
    if (log_stat.st_size > 0) {
        LOG(INFO) << "recover " << log_stat.st_size << "B from local";
        read(fd, buffer->getData(), log_stat.st_size);
        for (auto &p : peers)
            recoverPeer(p);
    }
}

void ClientWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx) {
    CSLClient *cli = reinterpret_cast<CSLClient *>(watcher_ctx);
    LOG(INFO) << "Client watcher triggered on client " << cli->GetId() << ", type: " << type2String(type)
              << " state: " << state2String(state) << " path: " << path;
    if (state == ZOO_CONNECTED_STATE) {
        if (type == ZOO_DELETED_EVENT) {
            const auto start = chrono::high_resolution_clock::now();

            string peer = getPeerFromPath(path), new_peer;
            
            if ((new_peer = cli->replacePeer(peer)).empty()) {
                LOG(WARNING) << "replacement failed";
                if (cli->peers.size() <= cli->rep_factor / 2) {
                    LOG(ERROR) << "less than half peers working, write suspended. cur: " << cli->peers.size();
                    cli->recover_lock.lock();
                    // set child watch on server root node to be informed of new server join
                    cli->watchForPeerJoin();
                } else {
                    LOG(WARNING) << "working under reduced redundancy, peer num: " << cli->peers.size()
                                 << ", expected num: " << cli->rep_factor;
                }
                return;
            }
            cli->recoverPeer(new_peer);

            const auto end = chrono::high_resolution_clock::now();
            LOG(INFO) << "Replace and recover a peer takes "
                      << chrono::duration_cast<chrono::microseconds>(end - start).count() << "us";
        } else if (type == ZOO_CHILD_EVENT) {
            string peer = "";
            string new_peer = cli->replacePeer(peer);
            cli->recoverPeer(new_peer);
            if (cli->peers.size() > cli->rep_factor / 2) {
                cli->recover_lock.unlock();
            }
            if (cli->peers.size() < cli->rep_factor) {
                cli->watchForPeerJoin();  // haven't recover to full redundancy, keep watching for new server join
            }
        }
    }
}
