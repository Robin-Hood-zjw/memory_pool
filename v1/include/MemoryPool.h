#pragma once

#include <mutex>
#include <atomic>
#include <cassert>

namespace Pool {
    // the smallest unit of memory in the pool
    struct Slot {
        std::atomic<Slot*> next;                        // when free, holds a  pointer to the next free slot; when allocated, holds user data
    };

    class MemoryPool {
    private:
        int _slotSize;                              // the size of each slot after alignment, must be >= sizeof(Slot)
        int _blockSize;                             // the size of each block, must be a multiple of _slotSize

        std::mutex _mutex;                          // protects access to the free list and block allocation

        Slot* _curSlot;                             // pointer to the current slot for allocation
        Slot* _lastSlot;                            // pointer to the last slot in the current block
        Slot* _firstBlock;                          // pointer to the first block of memory (for cleanup)
        std::atomic<Slot*> _freeList;               // head of the list of all the free slots

        void allocateNewBlock();                    // allocates a new memory block and sets up the free list with it
        size_t padPointer(char* p, size_t align);   // aligns the pointer p to the next multiple of align

        Slot* popFreeList();                        // pops a slot from the free list or returns nullptr if the free list is empty
        bool pushFreeList(Slot* slot);              // pushes a slot back to the free list, returns true if successfully added, false if already in
    public:
        MemoryPool(size_t blockSize = 4096);        // initializes the pool with the given block size (default 4KB)
        ~MemoryPool();                              // releases all memory blocks allocated by the pool

        void init(size_t);                          // initializes the pool to manage slots of the given size
        void* allocate();                           // allocates a slot and returns a pointer to its data area
        void deallocate(void*);                     // return the slot to the free list
    };
}