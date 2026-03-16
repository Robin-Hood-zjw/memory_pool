#pragma once

#include "./Common.h"

namespace Pool {
    class ThreadCache {
    private:
        std::array<void*, FREE_LIST_SIZE> _freeList;            // an array that stores the heads of free-memory linked lists based on various sizes
        std::array<size_t, FREE_LIST_SIZE> _freeListSize;       // an array that records the number of free blocks that are attached to each linked list

        ThreadCache();                                          // initialize two arrays with initial values

        void* fetchFromCentralCache(size_t index);              // request memory blocks from the central cache
        void returnToCentralCache(void* start, size_t size);    // return memory blocks to the central cache
        bool shouldReturnToCentralCache(size_t index);          // determine if to trigger the garbage collection logic

    public:
        static ThreadCache* getInstance();                      // ensure that each thread has its own private ThreadCache instance

        void* allocate(size_t size);                            // allocate memory blocks from the free list
        void deallocate(void* ptr, size_t size);                // deallocate memory blocks into the free list and return some into central cache
    };
}