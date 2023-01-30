#include "mr_pool.h"

#include <algorithm>

bool mr_cmp(const shared_ptr<Buffer> &b1, const shared_ptr<Buffer> &b2) {
    if (b1->getSizeInBytes() != b2->getSizeInBytes())
        return b1->getSizeInBytes() < b2->getSizeInBytes();
    else
        return b1 < b2;
}

NCLMrPool::NCLMrPool(Context *context) : context(context), free_mrs(mr_cmp) {}

shared_ptr<Buffer> NCLMrPool::GetMRofSize(size_t size) {
    lock_guard<mutex> guard(lock);
    if (free_mrs.empty()) {
        return make_shared<Buffer>(context, size);
    } else {
        auto it_mr = find_if(free_mrs.begin(), free_mrs.end(),
                             [size](const shared_ptr<Buffer> &b) -> bool { return b->getSizeInBytes() >= size; });
        auto mr = *it_mr;
        free_mrs.erase(it_mr);
        return mr;
    }
}

void NCLMrPool::RecycleMR(shared_ptr<Buffer> mr) {
    lock_guard<mutex> guard(lock);
    mr->zero();
    free_mrs.insert(mr);
}
