/*
 * Compute-side log RDMA server
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#include "server.h"

#include <glog/logging.h>

CSLServer::CSLServer(uint16_t port, size_t buf_size) : buf_size(buf_size) {
    context = new infinity::core::Context(0, 1);
    qp_factory = new infinity::queues::QueuePairFactory(context);

    LOG(INFO) << "Setting up connection (blocking)" << endl;
    qp_factory->bindToPort(port);
    LOG(INFO) << "Bind to port";
}

void CSLServer::Run() {
    while (true) {
        local_props.emplace_back();
        LocalConData &prop = local_props.back();
        LOG(INFO) << "Creating buffers to read from and write to" << endl;
        prop.buffer = new infinity::memory::Buffer(context, buf_size);
        prop.buffer_token = prop.buffer->createRegionToken();
        prop.qp = qp_factory->acceptIncomingConnection(prop.buffer_token, sizeof(*(prop.buffer_token)));
    }
}

CSLServer::~CSLServer() {
    for (auto &p : local_props) {
        if (p.buffer) delete p.buffer;
        if (p.qp) delete p.qp;
    }
    if (qp_factory) delete qp_factory;
    if (context) delete context;
}
