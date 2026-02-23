#pragma once

#include <array>
#include <atomic>

namespace Pool {
    constexpr size_t ALIGNMENT = 8;
    constexpr size_t PAGE_SIZE = 4096;
    constexpr size_t MAX_BYTES = 256 * 1024;
    constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT;

    struct BlockHeader {
        bool inUse;
        size_t size;
        BlockHeader* next;
    };

    class SizeClass {
        public:
            static size_t roundUp(size_t bytes);
            static size_t getIndex(size_t bytes);
    };
}