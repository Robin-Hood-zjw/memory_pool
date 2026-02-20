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

    void MemoryPool::init(size_t size) {
        _slotSize = size;
        _firstBlock = nullptr;
        _curSlot = nullptr;
        _lastSlot = nullptr;
        _freeList = nullptr;
    }

    Slot* MemoryPool::popFreeList() {
        while (true) {
            Slot* oldHead = _freeList.load(std::memory_order_acquire);
            if (!oldHead) return nullptr;

            Slot* newHead = nullptr;
            try {
                newHead = oldHead->next.load(std::memory_order_relaxed);
            } catch (...) {
                continue;
            }

            if (_freeList.compare_exchange_weak(oldHead, newHead, std::memory_order_acquire, std::memory_order_relaxed)) return oldHead;
        }
    }
}