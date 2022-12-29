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

#include "client.h"

CSLServer::CSLServer(uint16_t port, size_t buf_size, string mgr_addr, uint16_t mgr_port)
    : buf_size(buf_size), conn_cnt(0), stop(false) {
    context = new infinity::core::Context(0, 1);
    qp_factory = new infinity::queues::QueuePairFactory(context);
    int ret;

    LOG(INFO) << "Setting up connection (blocking)" << endl;
    qp_factory->bindToPort(port);
    LOG(INFO) << "Bind to port";

    if (mgr_addr == "") return;  // skip connect to zookeeper

    zh = zookeeper_init((mgr_addr + ":" + to_string(mgr_port)).c_str(), ServerWatcher, 10000, 0, this, 0);
    if (!zh) {
        LOG(ERROR) << "Failed to init zookeeper handler, errno: " << errno;
        return;
    }

    struct ACL acl[] = {{
        .perms = ZOO_PERM_ALL,
        .id = ZOO_ANYONE_ID_UNSAFE,
    }};
    struct ACL_vector aclv = {1, acl};
    string node_path = ZK_SVR_ROOT_PATH + "/" + qp_factory->getIpAddress();
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
    
    while (!stop) {
        FD_ZERO(&fds);
        FD_SET(listen_fd, &fds);

        ret = select(listen_fd + 1, &fds, nullptr, nullptr, &tv);
        if (ret < 0) {
            LOG(ERROR) << "Error select(), errno: " << ret;
            return;
        } else if (ret == 0) {
            continue;
        }

        DLOG_ASSERT(FD_ISSET(listen_fd, &fds)) << "select returns non-0, but fd not set";
        FD_CLR(listen_fd, &fds);

        int socket = 0;
        infinity::queues::serializedQueuePair *recv_buf;
        struct FileInfo fi;
        string filename;

        LOG(INFO) << "Waiting for connection from new client...";
        socket = qp_factory->waitIncomingConnection(&recv_buf);
        DLOG_ASSERT(recv_buf->userDataSize == sizeof(FileInfo))
            << "Incorrect user data size"
            << "expect " << sizeof(FileInfo) << " receive " << recv_buf->userDataSize;
        fi = *(reinterpret_cast<FileInfo *>(recv_buf->userData));
        filename = fi.filename;
        LOG(INFO) << "Get incoming connection: " << filename << ":" << fi.size / 1024.0 / 1024.0 << "MB";

        auto it = local_cons.find(filename);
        if (it == local_cons.end()) {
            LOG(INFO) << "Create new MR and qp";
            LocalConData con;
            con.buffer = new infinity::memory::Buffer(context, fi.size);
            memset(con.buffer->getData(), 0, con.buffer->getSizeInBytes());
            con.buffer_token = con.buffer->createRegionToken();

            con.qp =
                qp_factory->replyIncomingConnection(socket, recv_buf, con.buffer_token, sizeof(*(con.buffer_token)));
            local_cons.insert(make_pair(filename, con));
            conn_cnt++;
        } else {
            LOG(INFO) << "Reuse exist MR and recreate qp";
            LocalConData &con = it->second;
            delete con.qp;
            con.qp =
                qp_factory->replyIncomingConnection(socket, recv_buf, con.buffer_token, sizeof(*(con.buffer_token)));
        }
        LOG(INFO) << "Connection accepted, total: " << conn_cnt;
    }
}

vector<string> CSLServer::GetAllFilename() {
    vector<string> all_filename;
    for (auto &c : local_cons) {
        all_filename.emplace_back(c.first);
    }
    return all_filename;
}

CSLServer::~CSLServer() {
    if (zh) zookeeper_close(zh);
    for (auto &c : local_cons) {
        if (c.second.buffer) delete c.second.buffer;
        if (c.second.qp) delete c.second.qp;
    }
    if (qp_factory) delete qp_factory;
    if (context) delete context;
}

void ServerWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx) {
    LOG(INFO) << "Server watcher triggered, type: " << type << " state: " << state << " path: " << path;
}
