#pragma once

#include <array>
#include <atomic>

namespace Pool {
    constexpr size_t ALIGNMENT = 8;                             // the basic size of memory alignment benchmark
    constexpr size_t PAGE_SIZE = 4096;                          // the size of a memory page
    constexpr size_t MAX_BYTES = 256 * 1024;                    // the max size of a single object in the memory pool
    constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;    // the number of hash buckets after calculation

    struct BlockHeader {
        bool inUse;                                             // show the memory block is allocated(true) or free(false)
        size_t size;                                            // the actual size of this block
        BlockHeader* next;                                      // connects to the next free memory block
    };

    class SizeClass {
        public:
            static inline size_t roundUp(size_t bytes);         // align upwards
            static inline size_t getIndex(size_t bytes);        // route Index
    };
}