#include "../include/PageCache.h"

namespace Pool {
    void* PageCache::systemAlloc(size_t pageNum) {
        size_t size = pageNum * PAGE_SIZE;
        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) return nullptr;

        memset(ptr, 0, size);
        return ptr;
    }

    void* PageCache::allocateSpan(size_t pageNum) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto itr = _freeSpans.lower_bound(pageNum);

        if (itr != _freeSpans.end()) {
            Span* span = itr->second;

            if (span->next) {
                _freeSpans[itr->first] = span->next;
            } else {
                _freeSpans.erase(itr);
            }

            if (span->pageNum > pageNum) {
                char* address = static_cast<char*>(span->pageAddress);

                Span* newSpan = new Span;
                newSpan->pageAddress = address + pageNum * PAGE_SIZE;
                newSpan->pageNum = span->pageNum - pageNum;
                newSpan->next = nullptr;

                auto& list = _freeSpans[newSpan->pageNum];
                newSpan->next = list;
                list = newSpan;

                span->pageNum = pageNum;
            }

            _spanMap[span->pageAddress] = span;
            return span->pageAddress;
        }

        void* memory = systemAlloc(pageNum);
        if (!memory) return nullptr;

        Span* span = new Span;
        span->pageAddress = memory;
        span->pageNum = pageNum;
        span->next = nullptr;

        _spanMap[memory] = span;
        return memory; 
    }

    void PageCache::deallocateSpan(void* ptr, size_t pageNum) {
        std::lock_guard<std::mutex> lock(_mutex);

        auto itr = _spanMap.find(ptr);
        if (itr == _spanMap.end()) return;
        Span* span = itr->second;

        void* nextAddr = static_cast<char*>(ptr) + pageNum * PAGE_SIZE;
        auto nextItr = _spanMap.find(nextAddr);

        if (nextItr != _spanMap.end()) {
            bool found = false;
            Span* nextSpan = nextItr->second;
            auto& nextList = _freeSpans[nextSpan->pageNum];

            if (nextList == nextSpan) {
                nextList = nextSpan->next;
                found = true;
            } else {
                Span* prev = nextList;
                while (prev && prev->next) {
                    if (prev->next == nextSpan) {
                        prev->next = nextSpan->next;
                        found = true;
                        break;
                    }
                    prev = prev->next;
                }
            }

            if (found) {
                span->pageNum += nextSpan->pageNum;
                delete nextSpan;
                _spanMap.erase(nextItr);
            }

            auto& list = _freeSpans[span->pageNum];
            span->next = list;
            list = span;
        }

        auto& list = _freeSpans[span->pageNum];
        span->next = list;
        list = span;
    }
}