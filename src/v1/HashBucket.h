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

            static void* useMemory(size_t size);
            static void freeMemory(void* ptr, size_t size);

            template<typename T, typename... Args>
            friend T* newElement(Args&&... args);

            template<typename T>
            friend void deleteElement(T* p);
    };

    template<typename T, typename... Args>
    T* newElement(Args&&... args) {
        T* p = nullptr;
        p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)));

        if (p) new(p) T(std::forward<Args>(args)...);
        return p;
    }

    template<typename T>
    void deleteElement(T* p) {
        if (p) {
            p->~T();
            HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T))
        }
    }
}