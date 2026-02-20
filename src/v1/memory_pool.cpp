#include "./memory_pool.h"

namespace MemoryPool {
    MemoryPool::MemoryPool(size_t blockSize = 4096):
        _slotSize(0),
        _blockSize(blockSize),
        _firstBlock(nullptr),
        _curSlot(nullptr),
        _lastSlot(nullptr),
        _freeList(nullptr) {}

    MemoryPool::~MemoryPool() {
        Slot* cur = _firstBlock;

        while (cur) {
            Slot* next = cur->next;

            operator delete(reinterpret_cast<void*>(cur));
            cur = next;
        }
        
    }
}