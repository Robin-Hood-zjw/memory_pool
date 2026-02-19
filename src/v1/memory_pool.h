#pragma once

#include <mutex>
#include <atomic>

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
    std::atomic<Slot*> _freeSlot;

    void allocateNewBlock();

    size_t padPointer(char* p, size_t align);

    bool pushFreeList(Slot* slot);

    Slot* popFreeList();
public:
    MemoryPool(size_t blockSize = 4096);
    ~MemoryPool();
};
