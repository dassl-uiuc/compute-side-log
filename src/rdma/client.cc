/*
 * Compute-side log RDMA client
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */
#include "client.h"

#include <stdlib.h>
#include <glog/logging.h>

CSLClient::CSLClient(const char* hostAddress, uint16_t port, size_t buf_size) {
    context = new infinity::core::Context(0, 1);
    qp_factory = new infinity::queues::QueuePairFactory(context);

    LOG(INFO) << "Connecting to remote node" << endl;
    qp = qp_factory->connectToRemoteHost(hostAddress, port);
    remote_buffer_token = static_cast<infinity::memory::RegionToken *>(qp->getUserData());

    LOG(INFO) << "Creating buffers";
    if (posix_memalign(&(mem), infinity::core::Configuration::PAGE_SIZE, buf_size)) {
        LOG(FATAL) << "[INFINITY][MEMORY][BUFFER] Cannot allocate and align buffer.";
    }
    buffer_1side = new infinity::memory::Buffer(context, mem, buf_size);
}

CSLClient::~CSLClient() {
    if (buffer_1side)
        delete buffer_1side;
    if (qp_factory)
        delete qp_factory;
    if (context)
        delete context;
}

void CSLClient::read_sync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    infinity::requests::RequestToken request_token(context);
    qp->read(buffer_1side, local_off, remote_buffer_token, remote_off, size, &request_token);
    request_token.waitUntilCompleted();
}

void CSLClient::write_sync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    infinity::requests::RequestToken request_token(context);
    qp->write(buffer_1side, local_off, remote_buffer_token, remote_off, size, &request_token);
    request_token.waitUntilCompleted();
}
