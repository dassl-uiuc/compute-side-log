/*
 * Compute-side log RDMA client
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */
#include "client.h"

#include <glog/logging.h>
#include <stdlib.h>
#include <memory>

CSLClient::CSLClient(vector<const char *> hostAddresses, uint16_t port, size_t buf_size) : buf_offset(0) {
    context = new infinity::core::Context(0, 1);
    qp_factory = new infinity::queues::QueuePairFactory(context);

    LOG(INFO) << "Connecting to remote node" << endl;
    for (const char *addr : hostAddresses) {
        RemoteConData prop;
        prop.qp = qp_factory->connectToRemoteHost(addr, port);
        prop.remote_buffer_token = static_cast<infinity::memory::RegionToken *>(prop.qp->getUserData());
        remote_props.push_back(prop);
    }

    LOG(INFO) << "Creating buffers";
    if (posix_memalign(&(mem), infinity::core::Configuration::PAGE_SIZE, buf_size)) {
        LOG(FATAL) << "[INFINITY][MEMORY][BUFFER] Cannot allocate and align buffer.";
    }
    buffer = new infinity::memory::Buffer(context, mem, buf_size);
}

CSLClient::~CSLClient() {
    if (buffer) delete buffer;
    if (qp_factory) delete qp_factory;
    if (context) delete context;
}

void CSLClient::ReadSync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    infinity::requests::RequestToken request_token(context);
    RemoteConData &prop = remote_props[0];
    prop.qp->read(buffer, local_off, prop.remote_buffer_token, remote_off, size, &request_token);
    request_token.waitUntilCompleted();
}

void CSLClient::WriteSync(uint64_t local_off, uint64_t remote_off, uint32_t size) {
    vector<shared_ptr<infinity::requests::RequestToken> > request_tokens;
    for (auto &prop : remote_props) {
        auto token = make_shared<infinity::requests::RequestToken>(context);
        request_tokens.emplace_back(token);
        prop.qp->write(buffer, local_off, prop.remote_buffer_token, remote_off, size, token.get());
    }

    for (auto token : request_tokens) {
        token->waitUntilCompleted();
    }
}

void CSLClient::Append(void *buf, uint32_t size) {
    size_t cur_off = buf_offset.fetch_add(size);
    memcpy((char *)mem + cur_off, buf, size);
    WriteSync(cur_off, cur_off, size);
}
