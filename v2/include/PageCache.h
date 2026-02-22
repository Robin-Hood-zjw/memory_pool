#pragma once

#include "./Common.h"

#include <map>
#include <mutex>

namespace Pool {
    class PageCache {
        private:
            struct Span {
                void* pageAddress;
                size_t numPages;
                Span* next;
            };
            
            std::mutex _mutex;
            std::map<void*, Span*> _spanMap;
            std::map<size_t, Span*> _freeSpans;

            PageCache() = default;
            void* systemAlloc(size_t numPages);

        public:
            static PageCache& getInstance() {
                static PageCache instance;
                return instance;
            }

            void* allocateSpan(size_t numPages);
            void deallocateSpan(void* ptr, size_t numPages);
    };
}