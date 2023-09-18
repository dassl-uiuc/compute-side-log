#pragma once

#include <infinity/core/Context.h>
#include <infinity/memory/Buffer.h>

#include <memory>
#include <mutex>
#include <set>

using namespace std;
using infinity::core::Context;
using infinity::memory::Buffer;

bool mr_cmp(const shared_ptr<Buffer> &b1, const shared_ptr<Buffer> &b2);  // custom comparitor for MR pointer

class NCLMrPool {
   protected:
    Context *context;
    set<shared_ptr<Buffer>, decltype(mr_cmp) *> free_mrs;  // sorted by mr size
    mutex lock;

   public:
    /**
     * Construct a MR pool
     * 
     * @param pre_allocate number of MR to pre-allocate. This will save time in creating MR when client request to
     * replicate a new file
     */
    NCLMrPool(Context *context, int pre_allocate = 0);

    /**
     * Get a MR of particular size. If free mr of satisfied size is available, get the mr.
     * Else, create a new mr. Called when a new file is opened.
     *
     * @param size the lower size limit that the MR needs to satisfy
     * @return pointer to the mr
     */
    shared_ptr<Buffer> GetMRofSize(size_t size);

    /**
     * Recycle a mr when no longer needed. Called when a file is closed.
     * @param mr MR to be recycled
     */
    void RecycleMR(shared_ptr<Buffer> mr);
};
