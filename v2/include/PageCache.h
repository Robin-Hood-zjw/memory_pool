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
                void* pageAddress;
                size_t pageNum;
                Span* next;
            };
            
            std::mutex _mutex;
            std::map<void*, Span*> _spanMap;
            std::map<size_t, Span*> _freeSpans;

            PageCache() = default;
            void* systemAlloc(size_t pageNum);

        public:
            static PageCache& getInstance() {
                static PageCache instance;
                return instance;
            }

            void* allocateSpan(size_t pageNum);
            void deallocateSpan(void* ptr, size_t pageNum);
    };
}