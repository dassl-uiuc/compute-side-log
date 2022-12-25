/*
 * Compute-side log RDMA server
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#include "server.h"

#include <errno.h>
#include <glog/logging.h>

CSLServer::CSLServer(uint16_t port, size_t buf_size, string mgr_addr, uint16_t mgr_port)
    : buf_size(buf_size), conn_cnt(0) {
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
    string node_path = (ZK_ROOT_PATH + "/" + qp_factory->getIpAddress());
    int value = 0;
    ret = zoo_create(zh, ZK_ROOT_PATH.c_str(), (const char *)&value, sizeof(value), &aclv, ZOO_PERSISTENT, nullptr, 0);
    if (ret && ret != ZNODEEXISTS) {
        LOG(ERROR) << "Failed to create zk node: " << ZK_ROOT_PATH << ", errno: " << ret;
        return;
    }
    ret = zoo_create(zh, node_path.c_str(), (const char *)&value, sizeof(value), &aclv, ZOO_EPHEMERAL, nullptr, 0);
    if (ret) {
        LOG(ERROR) << "Failed to create zk node: " << node_path << ", errno: " << ret;
        return;
    }
}

void CSLServer::Run() {
    while (true) {
        local_props.emplace_back();
        LocalConData &prop = local_props.back();

        LOG(INFO) << "Creating buffers to read from and write to" << endl;
        prop.buffer = new infinity::memory::Buffer(context, buf_size);
        memset(prop.buffer->getData(), 0, prop.buffer->getSizeInBytes());
        prop.buffer_token = prop.buffer->createRegionToken();

        LOG(INFO) << "Waiting for connection from new client...";
        prop.qp = qp_factory->acceptIncomingConnection(prop.buffer_token, sizeof(*(prop.buffer_token)));
        conn_cnt++;
        LOG(INFO) << "Connection accepted, total: " << conn_cnt;
    }
}

CSLServer::~CSLServer() {
    if (zh) zookeeper_close(zh);
    for (auto &p : local_props) {
        if (p.buffer) delete p.buffer;
        if (p.qp) delete p.qp;
    }
    if (qp_factory) delete qp_factory;
    if (context) delete context;
}

void ServerWatcher(zhandle_t *zh, int type, int state, const char *path, void *watcher_ctx) {
    LOG(INFO) << "Server watcher triggered, type: " << type << " state: " << state << " path: " << path;
}
