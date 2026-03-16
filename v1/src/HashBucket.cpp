#include "../include/HashBucket.h"
#include "../include/MemoryPool.h"

namespace Pool {
    void HashBucket::initMemoryPool() {
        // initialize all the memory pools
        for (size_t i = 0; i < MEMORY_POOL_NUM; i++) {
            getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
        }
    }

    MemoryPool& HashBucket::getMemoryPool(int index) {
        // create an MemoryPool array of the target size
        static MemoryPool pools[MEMORY_POOL_NUM];
        // singleton pattern: get the target memory pool
        return pools[index];
    }

    void* HashBucket::useMemory(size_t size) {
        if (size <= 0) return nullptr;
        if (size > MAX_SLOT_SIZE) return operator new(size);
        

        // find the target memory pool and allocate a slot
        return getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).allocate();
    }

    void HashBucket::freeMemory(void* ptr, size_t size) {
        if (!ptr) return;
        if (size > MAX_SLOT_SIZE) {
            operator delete(ptr);
            return;
        }

        // find the target memory pool and deallocate the slot
        getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).deallocate(ptr);
    }
};