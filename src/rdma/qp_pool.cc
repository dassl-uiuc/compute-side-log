#include "qp_pool.h"
#include <sys/socket.h>
#include <string.h>

NCLQpPool::NCLQpPool(Context *context, const uint16_t port) : context(context), port(port) {
    qp_factory = make_shared<QueuePairFactory>(context);
}

shared_ptr<QueuePair> NCLQpPool::GetQpTo(const string &host_addr, struct FileInfo *fi) {
    lock_guard<mutex> guard(lock);
    if (idle_qps[host_addr].empty()) {
        auto qp = shared_ptr<QueuePair>(qp_factory->connectToRemoteHost(host_addr.c_str(), port, fi, sizeof(*fi)));
        return qp;
    } else {
        auto &idle_queue = idle_qps[host_addr];
        auto qp = idle_queue.front();
        idle_queue.pop();
        // exchange MR info with server
        struct ClientReq open_req;
        open_req.type = OPEN_FILE;
        open_req.fi.size = fi->size;
        open_req.fi.epoch = fi->epoch;
        memcpy(open_req.fi.file_id, fi->file_id, MAX_FILE_ID_LENGTH);
        send(qp->getRemoteSocket(), &open_req, sizeof(open_req), 0);
        recv(qp->getRemoteSocket(), qp->getUserData(), sizeof(RegionToken), 0);
        return qp;
    }
}

void NCLQpPool::RecycleQp(shared_ptr<QueuePair> qp) {
    lock_guard<mutex> guard(lock);
    idle_qps[qp->getRemoteAddr()].push(qp);
}
