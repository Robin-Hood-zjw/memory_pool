#include "../include/PageCache.h"

namespace Pool {
    /**
     * @brief allocates raw memory from the OS using memory mapping
     * @param pageNum the page number to request from the system
     * @return void* a pointer to the start of the mapped memory, or nullptr if failed
     **/
    void* PageCache::systemAlloc(size_t pageNum) {
        size_t size = pageNum * PAGE_SIZE;

        // request anonymous private memory mapping from the OS
        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) return nullptr;

        // zero out the memory to ensure clean
        memset(ptr, 0, size);
        return ptr;
    }

    /**
     * @brief thread-safe singleton accessor
     * @return PageCache& Reference to the global shared PageCache
     **/
    PageCache& PageCache::getInstance() {
        static PageCache instance;
        return instance;
    }

    /**
     * @brief requests a Span of memory, and splits larger Spans if no such match
     * @param pageNum the contiguous page number
     * @return void* the start address of the allocated Span
     **/
    void* PageCache::allocateSpan(size_t pageNum) {
        // safeguard: protect the global thread
        std::lock_guard<std::mutex> lock(_mutex);

        // find the smallest span (>= pageNum)
        auto itr = _freeSpans.lower_bound(pageNum);
        if (itr != _freeSpans.end()) {
            Span* span = itr->second;

            // remove the span from the free list, or erase the list if no following span
            if (span->next) {
                _freeSpans[itr->first] = span->next; 
            } else {
                _freeSpans.erase(itr);
            }

            // split the span if larger than the request
            if (span->pageNum > pageNum) {
                // get the address of this span
                char* address = static_cast<char*>(span->pageAddress);

                // initialize a new span with standardizations after splitting
                Span* newSpan = new Span;
                newSpan->pageAddress = address + pageNum * PAGE_SIZE;
                newSpan->pageNum = span->pageNum - pageNum;
                newSpan->next = nullptr;

                // put the remainder back into the free list of its new size
                auto& list = _freeSpans[newSpan->pageNum];
                newSpan->next = list;
                list = newSpan;

                // update the size of the old span
                span->pageNum = pageNum;
            }

            _spanMap[span->pageAddress] = span;
            return span->pageAddress; 
        }

        // request new memory from OS if no span found in cache
        void* memory = systemAlloc(pageNum);
        //safeguard: return NULL if no memory allocated
        if (!memory) return nullptr;

        // initialize a new span with such standardizations
        Span* span = new Span;
        span->pageAddress = memory;
        span->pageNum = pageNum;
        span->next = nullptr;
        // record span into "_spanMap"
        _spanMap[memory] = span;

        return span;
    }

    /**
     * @brief returns a span to the cache and attempts to merge it with adjacent free spans
     * @param ptr the start address of the memory to return
     * @param pageNum the memory size being returned in pages
     **/
    void PageCache::deallocateSpan(void* ptr, size_t pageNum) {
        // safeguard: protect the global thread
        std::lock_guard<std::mutex> lock(_mutex);

        // find the span iterator in "_spanMap"
        auto itr = _spanMap.find(ptr);
        // do nothing if no such match
        if (itr != _spanMap.end()) return;
        //get the span if found
        Span* span = itr->second;

        // calculate the address of the target span
        void* nextAddr = static_cast<char*>(span->pageAddress) + pageNum * PAGE_SIZE;

        // merge two spans
        auto nextItr = _spanMap.find(nextAddr);
        if (nextItr != _spanMap.end()) {
            bool found = false;
            Span* nextSpan = nextItr->second;
            auto& nextList = _freeSpans[nextSpan->pageNum];

            if (nextSpan == nextList) {
                nextList = nextSpan->next;
                found = true;
            } else if (nextList) {
                Span* prev = nextList;

                // loop to find if "nextSpan" exists in the free list
                while (prev && prev->next) {
                    if (prev->next == nextSpan) {
                        prev->next = nextSpan->next;
                        found = true;
                        break;
                    }

                    prev = prev->next;
                }
            }

            // merge two spans if "nextSpan" is found in the free list
            if (found) {
                span->pageNum += nextSpan->pageNum;
                _spanMap.erase(nextItr);
                delete nextSpan;
            }
        }

        // update the head of "_freeSpans"
        auto& list = _freeSpans[span->pageNum];
        span->next = list;
        list = span;
    }
}