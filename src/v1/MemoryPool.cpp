#include "./MemoryPool.h"

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
        assert(size > 0);

        _slotSize = size;
        _firstBlock = nullptr;
        _curSlot = nullptr;
        _lastSlot = nullptr;
        _freeList = nullptr;
    }

    void* MemoryPool::allocate() {
        Slot* slot = popFreeList();
        if (slot) return slot;

        Slot* temp;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_curSlot >= _lastSlot) allocateNewBlock();
            temp = _curSlot;
            _curSlot += _slotSize / sizeof(Slot);
        }

        return temp;
    }

    void MemoryPool::deallocate(void* ptr) {
        if (!ptr) return;

        Slot* slot = reinterpret_cast<Slot*>(ptr);
        pushFreeList(slot);
    }

    void MemoryPool::allocateNewBlock() {
        void* newBlock = operator new(_blockSize);
        reinterpret_cast<Slot*>(newBlock)->next = _firstBlock;
        _firstBlock = reinterpret_cast<Slot*>(newBlock);

        char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
        size_t paddingSize = padPointer(body, _slotSize);
        _curSlot = reinterpret_cast<Slot*>(body + paddingSize);

        _lastSlot = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + _blockSize - _slotSize + 1);
    }

    size_t MemoryPool::padPointer(char* p, size_t align) {
        size_t rem = reinterpret_cast<size_t>(p) % align;
        return rem == 0 ? 0 : align - rem;
    }

    Slot* MemoryPool::popFreeList() {
        while (true) {
            Slot* oldHead = _freeList.load(std::memory_order_acquire);
            if (!oldHead) return nullptr;

            Slot* newHead = nullptr;
            try {
                newHead = oldHead->next.load(std::memory_order_relaxed);
            } catch(...) {
                continue;
            }

            if (_freeList.compare_exchange_weak(
                    oldHead, newHead, 
                    std::memory_order_acquire, 
                    std::memory_order_relaxed
                )) return oldHead;
        }
        
    }
}