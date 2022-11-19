/*
 * Compute-side log RDMA client
 *
 * Copyright 2022 UIUC
 * Author: Xuhao Luo
 */

#pragma once

#include <infinity/core/Configuration.h>
#include <infinity/core/Context.h>
#include <infinity/memory/Buffer.h>
#include <infinity/memory/RegionToken.h>
#include <infinity/queues/QueuePair.h>
#include <infinity/queues/QueuePairFactory.h>

using namespace std;

class CSLClient {
   private:
    infinity::core::Context *context;
    infinity::queues::QueuePairFactory *qp_factory;
    infinity::queues::QueuePair *qp;
    infinity::memory::RegionToken *remote_buffer_token;
    infinity::memory::Buffer *buffer_1side;

    void* mem;

   public:
    CSLClient(const char *hostAddress, uint16_t port, size_t buf_size);
    ~CSLClient();

    void WriteSync(uint64_t local_off, uint64_t remote_off, uint32_t size);
    void ReadSync(uint64_t local_off, uint64_t remote_off, uint32_t size);

    void Append(void *buf, uint32_t size);
};
