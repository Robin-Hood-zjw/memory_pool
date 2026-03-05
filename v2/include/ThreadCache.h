#pragma once

#include "./Common.h"

namespace Pool {
    class ThreadCache {
        private:
            std::array<void*, FREE_LIST_SIZE> _freeList;
            std::array<size_t, FREE_LIST_SIZE> _freeListSize;

            ThreadCache() {
                _freeList.fill(nullptr);
                _freeListSize.fill(0);
            }

            void* fetchFromCentralCache(size_t index);
            void returnToCentralCache(void* start, size_t size);
            bool shouldReturnToCentralCache(size_t index);

        public:
            static ThreadCache* getInstance() {
                static thread_local ThreadCache instance;
                return &instance;
            }

            void* allocate(size_t size);
            void deallocate(void* ptr, size_t size);
    };
}