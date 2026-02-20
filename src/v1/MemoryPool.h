#pragma once

#include <mutex>
#include <atomic>
#include <cassert>

namespace MemoryPool {
    #define SLOT_BASE_SIZE 8
    #define MAX_SLOT_SIZE 512
    #define MEMORY_POOL_NUM 64

    struct Slot {
        std::atomic<Slot*> next;
    };

    class MemoryPool {
        private:
            int _slotSize;
            int _blockSize;
            std::mutex _mutex;

            Slot* _curSlot;
            Slot* _lastSlot;
            Slot* _firstBlock;
            std::atomic<Slot*> _freeList;

            void allocateNewBlock();
            size_t padPointer(char* p, size_t align);

            Slot* popFreeList();
            bool pushFreeList(Slot* slot);
        public:
            MemoryPool(size_t blockSize = 4096);
            ~MemoryPool();

            void init(size_t);
            void* allocate();
            void deallocate(void*);
    };
}