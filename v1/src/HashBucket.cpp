#include "../include/HashBucket.h"
#include "../include/MemoryPool.h"

namespace MemoryPool {
    void HashBucket::initMemoryPool() {
        for (int i = 0; i < MEMORY_POOL_NUM; i++) {
            getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
        }
    }

    MemoryPool& HashBucket::getMemoryPool(int index) {
        static MemoryPool memoryPool[MEMORY_POOL_NUM];
        return memoryPool[index];
    }

    void* HashBucket::useMemory(size_t size) {
        if (size <= 0) return nullptr;
        if (size > MAX_SLOT_SIZE) return operator new(size);

        return getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).allocate();
    }

    void HashBucket::freeMemory(void* ptr, size_t size) {
        if (!ptr) return;
        if (size > MAX_SLOT_SIZE) {
            operator delete(ptr);
            return;
        }

        getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).deallocate(ptr);
    }
};