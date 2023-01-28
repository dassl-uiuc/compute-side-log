#pragma once

#include <infinity/core/Context.h>
#include <infinity/memory/RegionToken.h>
#include <infinity/queues/QueuePair.h>
#include <infinity/queues/QueuePairFactory.h>

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "common.h"

using namespace std;
using infinity::core::Context;
using infinity::memory::RegionToken;
using infinity::queues::QueuePair;
using infinity::queues::QueuePairFactory;

class NCLQpPool {
   protected:
    map<string, queue<shared_ptr<QueuePair>>> idle_qps;

    Context *context;
    shared_ptr<QueuePairFactory> qp_factory;
    const uint16_t port;
    mutex lock;

   public:
    NCLQpPool(Context *context, const uint16_t port);

    /**
     * Get a qp to a replication server. If free qp to that server is available, get the free qp.
     * Else, create a new qp. Called when a new file is opened.
     *
     * @param host_addr address of the replication server
     * @param fi file info of the replicated file
     * @return pointer to the qp
     */
    shared_ptr<QueuePair> GetQpTo(const string &host_addr, struct FileInfo *fi);

    /**
     * Recycle a qp when no longer needed. Called when a file is closed.
     * @param qp QP to be recycled
     */
    void RecycleQp(shared_ptr<QueuePair> qp);
    Context *GetContext() { return context; }
};
