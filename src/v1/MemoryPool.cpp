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

    void MemoryPool::allocateNewBlock() {
        void* newBlock = operator new(_blockSize);
        reinterpret_cast<Slot*>(newBlock)->next = _firstBlock;
        _firstBlock = reinterpret_cast<Slot*>(newBlock);

        char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
        size_t paddingSize = padPointer(body, _slotSize);
        _curSlot = reinterpret_cast<Slot*>(body + paddingSize);

        _lastSlot = 
    }
}