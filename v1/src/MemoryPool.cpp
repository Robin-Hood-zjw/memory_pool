#include "../include/MemoryPool.h"

namespace Pool {
    MemoryPool::MemoryPool(size_t blockSize):
        _slotSize(0), _blockSize(blockSize), _curSlot(nullptr), _lastSlot(nullptr), _freeList(nullptr), _firstBlock(nullptr) {}

    MemoryPool::~MemoryPool() {
        Slot* cur = _firstBlock;

        // loop to release all the allocated blocks
        while (cur) {
            Slot* next = cur->next;
            operator delete(reinterpret_cast<void*>(cur));
            cur = next;
        }
    }

    void MemoryPool::init(size_t size) {
        assert(size > 0);
        _slotSize = size;
        _curSlot = nullptr;
        _lastSlot = nullptr;
        _freeList = nullptr;
        _firstBlock = nullptr;
    }

    void* MemoryPool::allocate() {
        // try to pop a slot from the free list of available slots
        Slot* slot = popFreeList();
        if (slot) return reinterpret_cast<void*>(slot);

        Slot* temp;
        {
            // lock to protect simultaneous multi-threads changing
            std::lock_guard<std::mutex> lock(_mutex);

            // if no free slots, allocate a new block
            if (_curSlot >= _lastSlot) allocateNewBlock();

            // move _curSlot pointing to the next slot position
            temp = _curSlot;
            _curSlot += _slotSize / sizeof(Slot);
        };

        return reinterpret_cast<void*>(temp);
    }

    void MemoryPool::deallocate(void* ptr) {
        // if ptr valid, convert it into a slot and push into the free list of slots
        if (ptr) {
            Slot* slot = reinterpret_cast<Slot*>(ptr);
            pushFreeList(slot);
        }
    }

    void MemoryPool::allocateNewBlock() {
        // allocate a block of memory
        char* newBlock = reinterpret_cast<char*>(operator new(_blockSize));

        // the head of the new block points to the next block and renew "_firstBlock"
        reinterpret_cast<Slot*>(newBlock)->next = _firstBlock;
        _firstBlock = reinterpret_cast<Slot*>(newBlock);

        // points to the first available address that serves Slot allocation
        char* body = reinterpret_cast<char*>(_firstBlock) + sizeof(Slot*);

        // align the block to the multiple of "_slotSize"
        // renew the pointer to the first available slot
        size_t padding = padPointer(body, _slotSize);
        _curSlot = reinterpret_cast<Slot*>(body + padding);

        // renew the last available slot's position ("_lastSlot")
        _lastSlot = reinterpret_cast<Slot*>(newBlock + _blockSize - _slotSize);
    }

    size_t MemoryPool::padPointer(char* p, size_t align) {
        size_t gap = reinterpret_cast<size_t>(p) % align;
        return gap == 0 ? gap : align - gap;
    }

    Slot* MemoryPool::popFreeList() {
        while (true) {
            // get the old head of the free list
            Slot* oldHead = _freeList.load(std::memory_order_acquire);
            if (!oldHead) return nullptr;

            // get the next slot from the old head of the free list
            Slot* newHead = nullptr;
            try {
                newHead = oldHead->next.load(std::memory_order_relaxed);
            } catch(...) {
                continue;
            }

            // renew the head of the free list with the next available slot
            // if successful, renew the "_freeList" with the "newHead"
            // if fail, automatically restart this loop and renew the "oldHead" with the "_freeList"
            if (_freeList.compare_exchange_weak(
                oldHead, newHead, 
                std::memory_order_acquire, std::memory_order_relaxed)
                ) return oldHead;
        }
    }

    bool MemoryPool::pushFreeList(Slot* slot) {
        while (true) {
            // get the head of the free list
            Slot* oldHead = _freeList.load(std::memory_order_relaxed);

            // link the the free list to the old head of the free list
            slot->next.store(oldHead, std::memory_order_relaxed);

            // renew the head of the free list with the slot
            // if successful, renew the "_freeList" with the "slot" 
            // if failed, automatically restart this loop and renew the "oldHead" with the "_freeList"
            if (_freeList.compare_exchange_weak(
                oldHead, slot, 
                std::memory_order_release, std::memory_order_relaxed)
                ) return true;
        }
    }
}