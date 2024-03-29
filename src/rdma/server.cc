/*
 * Compute-side log RDMA server
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#include "server.h"

#include <errno.h>
#include <glog/logging.h>
#include <sys/select.h>

#include "common.h"

using infinity::memory::RegionToken;
using infinity::queues::QueuePair;
using infinity::queues::QueuePairFactory;
using namespace std::chrono;

CSLServer::CSLServer(uint16_t port, size_t buf_size, string mgr_hosts) : stop(false) {
    context = new infinity::core::Context(infinity::core::Configuration::DEFAULT_IB_DEVICE,
                                          infinity::core::Configuration::DEFAULT_IB_PHY_PORT);
    qp_factory = new QueuePairFactory(context);
    mr_pool = make_unique<NCLMrPool>(context);
    int ret;

    LOG(INFO) << "Setting up connection (blocking)" << endl;
    qp_factory->bindToPort(port);
    LOG(INFO) << "Bind to port";

    if (mgr_hosts == "") return;  // skip connect to zookeeper

    zh = zookeeper_init(mgr_hosts.c_str(), ServerWatcher, 10000, 0, this, 0);
    if (!zh) {
        LOG(ERROR) << "Failed to init zookeeper handler, errno: " << errno;
        return;
    }

    struct ACL acl[] = {{
        .perms = ZOO_PERM_ALL,
        .id = ZOO_ANYONE_ID_UNSAFE,
    }};
    struct ACL_vector aclv = {1, acl};
    string node_path = ZK_SVR_ROOT_PATH + "/" + QueuePairFactory::getIpAddress();
    int value = 0;
    ret = zoo_create(zh, ZK_SVR_ROOT_PATH.c_str(), (const char *)&value, sizeof(value), &aclv, ZOO_PERSISTENT, nullptr,
                     0);
    if (ret && ret != ZNODEEXISTS) {
        LOG(ERROR) << "Failed to create zk node: " << ZK_SVR_ROOT_PATH << ", errno: " << ret;
        return;
    }
    ret = zoo_create(zh, node_path.c_str(), (const char *)&value, sizeof(value), &aclv, ZOO_EPHEMERAL, nullptr, 0);
    if (ret) {
        LOG(ERROR) << "Failed to create zk node: " << node_path << ", errno: " << ret;
        return;
    }
}

void CSLServer::Run() {
    fd_set fds;
    struct timeval tv = {
        .tv_sec = 1,
        .tv_usec = 0,
    };
    int listen_fd = qp_factory->getServerSocket();
    int ret;
    int max_fd;
    // set<int> client_socks;

    while (!stop) {
        FD_ZERO(&fds);
        FD_SET(listen_fd, &fds);
        max_fd = listen_fd;
        for (auto &qp : existing_qps) {
            if (qp.first > 0) {
                max_fd = max(max_fd, qp.first);  // find the max of all active fds
                FD_SET(qp.first, &fds);
            }
        }

        tv.tv_sec = 1;
        ret = select(max_fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) {
            LOG(ERROR) << "Error select(), errno: " << errno;
            return;
        } else if (ret > 0) {
            // check for incoming connection
            if (FD_ISSET(listen_fd, &fds)) {
                FD_CLR(listen_fd, &fds);
                handleIncomingConnection();
            }
            // then check for client requests
            for (auto qp = existing_qps.begin(); qp != existing_qps.end();) {
                if (qp->first > 0 && FD_ISSET(qp->first, &fds)) {
                    FD_CLR(qp->first, &fds);
                    ret = handleClientRequest(qp->first);
                    if (ret == 0) {
                        existing_qps.erase(qp++);  // connection has been terminated, prevent triggerring select again
                        continue;
                    }
                }
                ++qp;
            }
        } else {
            continue;
        }
    }
}

void CSLServer::handleIncomingConnection() {
    int socket = 0;
    infinity::queues::serializedQueuePair *recv_buf;
    struct FileInfo fi;
    string file_id;

#ifdef LATENCY
    auto start = high_resolution_clock::now();
#endif

    LOG(INFO) << "Waiting for connection from new client...";
    socket = qp_factory->waitIncomingConnection(&recv_buf);
    DLOG_ASSERT(recv_buf->userDataSize == sizeof(FileInfo))
        << "Incorrect user data size, "
        << "expect " << sizeof(FileInfo) << " receive " << recv_buf->userDataSize;
    // Get file information from client
    fi = *(reinterpret_cast<FileInfo *>(recv_buf->userData));
    file_id = fi.file_id;
    LOG(INFO) << "Get incoming connection: " << file_id << ":" << fi.size / 1024.0 / 1024.0 << "MB";

#ifdef LATENCY
    auto after_recv = high_resolution_clock::now();
#endif

    // Find if MR and QP have been created for this file
    auto it = local_cons.find(file_id);
    if (it == local_cons.end()) {
        LOG(INFO) << "Create new MR and qp";
        LocalConData con;
        con.socket = socket;
        con.buffer = mr_pool->GetMRofSize(fi.size);
        memset(con.buffer->getData(), 0, con.buffer->getSizeInBytes());
        con.buffer_token = shared_ptr<RegionToken>(con.buffer->createRegionToken());

        con.qp = shared_ptr<QueuePair>(
            qp_factory->replyIncomingConnection(socket, recv_buf, con.buffer_token.get(), sizeof(*(con.buffer_token))));
        con.epoch = fi.epoch;
        existing_qps.insert(make_pair(con.qp->getRemoteSocket(), con.qp));
        local_cons.insert(make_pair(file_id, con));
        // conn_cnt++;
    } else {
        /*
         * MR and QP has already been created and not freed/recycled
         * Client may disconnect abnormally
         */
        LOG(INFO) << "Reuse exist MR and recreate qp";
        LocalConData &con = it->second;
        con.socket = socket;
        // delete old QP as it has been disconnected
        existing_qps.erase(con.qp->getRemoteSocket());  // ? how to reuse a qp if it's disconnected?
        con.qp = shared_ptr<QueuePair>(
            qp_factory->replyIncomingConnection(socket, recv_buf, con.buffer_token.get(), sizeof(*(con.buffer_token))));
        existing_qps.insert(make_pair(con.qp->getRemoteSocket(), con.qp));
    }
    LOG(INFO) << "Connection accepted, total: " << GetConnectionCount();
#ifdef LATENCY
    auto after_reply = high_resolution_clock::now();
    printf("accept & recv %ldus\nreply %ldus\n", duration_cast<microseconds>(after_recv - start).count(),
           duration_cast<microseconds>(after_reply - after_recv).count());
#endif
}

int CSLServer::handleClientRequest(int socket) {
    ClientReq req;
    ServerResp resp;
    int ret;

    ret = recv(socket, &req, sizeof(req), 0);
    if (ret <= 0) return ret;
    DLOG_ASSERT(ret == sizeof(req)) << "Incorrect client request size, "
                                    << "expect " << sizeof(req) << " receive " << ret;

    string file_id(req.fi.file_id);
    auto it = local_cons.find(file_id);
    auto it_qp = existing_qps.find(socket);
    LocalConData new_con;
    switch (req.type) {
        case OPEN_FILE:
            if (it != local_cons.end()) {
                DLOG_ASSERT(socket == it->second.qp->getRemoteSocket()) << "socket unmatch";
                send(it->second.qp->getRemoteSocket(), it->second.buffer_token.get(), sizeof(RegionToken), 0);
            } else if (it_qp == existing_qps.end()) {
                LOG(ERROR) << "[OPEN FILE] Can't find the existing qp with the client";
                break;
            } else {
                DLOG_ASSERT(socket == it_qp->second->getRemoteSocket()) << "socket unmatch";
                new_con.qp = it_qp->second;
                new_con.buffer = mr_pool->GetMRofSize(req.fi.size);
                new_con.buffer_token = shared_ptr<RegionToken>(new_con.buffer->createRegionToken());
                new_con.socket = socket;
                local_cons.insert(make_pair(file_id, new_con));
                send(socket, new_con.buffer_token.get(), sizeof(RegionToken), 0);
            }

            break;
        case CLOSE_FILE:
            if (it == local_cons.end()) {
                LOG(ERROR) << "[CLOSE FILE] can't find file id: " << file_id;
                break;
            }
            finalizeConData(it->second);
            local_cons.erase(it);
            LOG(INFO) << "[CLOSE FILE] File: " << file_id << " finalized, return v " << ret;
            break;
        case EXIT_PROC:
            if (it == local_cons.end()) {
                LOG(ERROR) << "[EXIT PROC] can't find file id: " << file_id;
            } else {
                // finalizeConData(it->second);
                // local_cons.erase(it);
            }
            if (it_qp == existing_qps.end()) {
                LOG(ERROR) << "[EXIT PROC] can't find QP with socket: " << socket;
                break;
            }
            ret = 0;
            LOG(INFO) << "[EXIT PROC] File: " << file_id << " finalized with QP (socket=" << socket << ") destroyed";
            break;
        case GET_INFO:
            if (it != local_cons.end()) {
                resp.size = findSize(file_id);
                resp.seq = ReadSeqNum(file_id);
            } else {
                LOG(ERROR) << "[GET INFO] can't find file id: " << file_id;
            }
            send(it->second.qp->getRemoteSocket(), &resp, sizeof(resp), 0);
            break;
        case SYNC_PEER:
            if (it == local_cons.end()) {
                LOG(ERROR) << "[SYNC PEER] can't find file id: " << file_id;
                break;
            }
            if (req.fi.size != it->second.buffer->getSizeInBytes()) {
                LOG(WARNING) << "[SYNC PEER] req size not consistent with buf size, req: " << req.fi.size
                             << ", buf: " << it->second.buffer->getSizeInBytes();
            }
            it->second.tmp_buffer = mr_pool->GetMRofSize(it->second.buffer->getSizeInBytes());
            it->second.tmp_buffer_token.reset(it->second.tmp_buffer->createRegionToken());
            send(it->second.socket, it->second.tmp_buffer_token.get(), sizeof(RegionToken), 0);
            break;
        case SYNC_PEER_DONE:
            if (it == local_cons.end()) {
                LOG(ERROR) << "[SYNC PEER DONE] can't find file id: " << file_id;
                break;
            }
            it->second.buffer.swap(it->second.tmp_buffer);
            it->second.buffer_token.swap(it->second.tmp_buffer_token);
            mr_pool->RecycleMR(it->second.tmp_buffer);
            break;
        default:
            LOG(ERROR) << "Unknown request type" << req.type;
            break;
    }

    return ret;
}

void CSLServer::finalizeConData(struct LocalConData &con) {
    // * qp are never freed for now
    // delete con.qp;
    mr_pool->RecycleMR(con.buffer);
}

vector<string> CSLServer::GetAllFileId() {
    vector<string> all_file_id;
    for (auto &c : local_cons) {
        all_file_id.emplace_back(c.first);
    }
    return all_file_id;
}

uint64_t CSLServer::ReadSeqNum(const string &fileid) {
    auto buf = local_cons[fileid].buffer;
    return *reinterpret_cast<uint64_t *>(buf->getAddress() + buf->getSizeInBytes() - sizeof(uint64_t));
}

void CSLServer::Preload(ifstream &file) {

}

size_t CSLServer::findSize(const string &file_id) {
    if (local_cons.find(file_id) == local_cons.end()) return 0;

    auto &mr = local_cons[file_id].buffer;
    char *mr_begin = reinterpret_cast<char *>(mr->getData());
    char *buf = mr_begin + mr->getSizeInBytes() - sizeof(uint64_t) - 1;  // points to the end of the MR, ignore sequence number
    while (buf >= mr_begin) {
        if (*buf != 0) break;  // todo
        buf--;
    }
    return buf - mr_begin + 1;
}

CSLServer::~CSLServer() {
    if (zh) zookeeper_close(zh);
    // for (auto &c : local_cons) {
    //     if (c.second.buffer) delete c.second.buffer;
    //     if (c.second.qp) delete c.second.qp;
    // }
    if (qp_factory) delete qp_factory;
    if (context) delete context;
}

void ServerWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx) {
    LOG(INFO) << "Server watcher triggered, type: " << type << " state: " << state << " path: " << path;
}
