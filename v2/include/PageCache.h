#pragma once

#include "./Common.h"

#include <map>
#include <mutex>
#include <cstring>
#include <sys/mman.h>

namespace Pool {
    class PageCache {
    private:
        struct Span {
            void* pageAddress;                              // the starting address of the memory block
            size_t pageNum;                                 // the number of consecutive pages in this memory
            Span* next;                                     // a pointer used to chain the Spans of the same size
        };
        
        std::mutex _mutex;                                  // global lock: PageCache is shared across all threads
        std::map<void*, Span*> _spanMap;                    // a mapping where the key is a memory address and the value is a Span obejct
        std::map<size_t, Span*> _freeSpans;                 // A mapping where the key is a page count and the value is a linked list of Spans of the same size

        PageCache() = default;                              // default constructor
        void* systemAlloc(size_t pageNum);                  // low-level system call to request memory from the OS

    public:
        static PageCache& getInstance();                    // singleton: only one PageCache exists for the entire process

        void* allocateSpan(size_t pageNum);                 // offer a span of a specific page size
        void deallocateSpan(void* ptr, size_t pageNum);     // return a span into the cache and merge it with neighbors
    };
}