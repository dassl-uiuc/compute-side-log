#include "mr_pool.h"

#include <algorithm>

#include "../csl_config.h"

bool mr_cmp(const shared_ptr<Buffer> &b1, const shared_ptr<Buffer> &b2) {
    if (b1->getSizeInBytes() != b2->getSizeInBytes())
        return b1->getSizeInBytes() < b2->getSizeInBytes();
    else
        return b1 < b2;
}

NCLMrPool::NCLMrPool(Context *context, int pre_allocate) : context(context), free_mrs(mr_cmp) {
    for (int i = 0; i < pre_allocate; i++) {
        auto mr = make_shared<Buffer>(context, MR_SIZE);
        mr->zero();
        free_mrs.emplace(mr);
    }
}

shared_ptr<Buffer> NCLMrPool::GetMRofSize(size_t size) {
    lock_guard<mutex> guard(lock);
    if (free_mrs.empty()) {
        auto mr = make_shared<Buffer>(context, size);
        mr->zero();
        return mr;
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
