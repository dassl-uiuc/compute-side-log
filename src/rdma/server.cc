/*
 * Compute-side log RDMA server
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#include "server.h"

#include <glog/logging.h>

CSLServer::CSLServer(uint16_t port, size_t  buf_size) {
    context = new infinity::core::Context(0, 1);
    qp_factory = new infinity::queues::QueuePairFactory(context);

    LOG(INFO) << "Creating buffers to read from and write to" << endl;
    buffer = new infinity::memory::Buffer(context, buf_size);
    buffer_token = buffer->createRegionToken();

    LOG(INFO) << "Setting up connection (blocking)" << endl;
    qp_factory->bindToPort(port);
    LOG(INFO) << "Bind to port";
    qp = qp_factory->acceptIncomingConnection(buffer_token, sizeof(*buffer_token));
}

CSLServer::~CSLServer() {
    if (buffer)
        delete buffer;
    if (qp_factory)
        delete qp_factory;
    if (context)
        delete context;
}
