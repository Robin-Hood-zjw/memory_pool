#pragma once

#include "./MemoryPool.h"

namespace MemoryPool {
    #define SLOT_BASE_SIZE 8
    #define MAX_SLOT_SIZE 512
    #define MEMORY_POOL_NUM 64

    class HashBucket {
        public:
            static void initMemoryPool();
            static MemoryPool& getMemoryPool(int index);

            static void* useMemory(size_t size) {
                if (size <= 0) return nullptr;
                if (size > MAX_SLOT_SIZE) return operator new(size);

                return getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).allocate();
            }

            static void freeMemory(void* ptr, size_t size) {
                if (!ptr) return;
                if (size > MAX_SLOT_SIZE) {
                    operator delete(ptr);
                    return;
                }

                getMemoryPool((size + 7) / SLOT_BASE_SIZE - 1).deallocate(ptr);
            }

            template<typename T, typename... Args>
            friend T* newElement(Args&&... args);

            template<typename T>
            friend void deleteElement(T* p);
    };
}