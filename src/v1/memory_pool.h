#pragma once

#include <mutex>
#include <atomic>

namespace MemoryPool {
    #define SLOT_BASE_SIZE 8
    #define MAX_SLOT_SIZE 512

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

            Slot* popFreeList();

        public:
            MemoryPool(size_t blockSize = 4096);
            ~MemoryPool();

            void init(size_t);
            void* allocate();
    };
}