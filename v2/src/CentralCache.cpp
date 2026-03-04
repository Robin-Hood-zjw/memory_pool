#include "../include/PageCache.h"
#include "../include/CentralCache.h"

namespace Pool {
    const std::chrono::milliseconds CentralCache::DELAY_INTERVAL{1000};
    static const size_t SPAN_PAGES = 8;

    CentralCache::CentralCache() {
        for (auto& lock : _locks) lock.clear();
        for (auto& ptr : _centralFreeList) 
            ptr.store(nullptr, std::memory_order_relaxed);
        for (auto& cnt : _delayCounts)
            cnt.store(0, std::memory_order_relaxed);
        for (auto& time : _lastReturnTimes)
            time = std::chrono::steady_clock::now();

        _spanCount.store(0, std::memory_order_relaxed);
    };

    void* CentralCache::fetchRange(size_t index) {
        if (index >= FREE_LIST_SIZE) return nullptr;

        while (_locks[index].test_and_set(std::memory_order_acquire)) 
            std::this_thread::yield();

        void* result = nullptr;
        try {
            result = _centralFreeList[index].load(std::memory_order_relaxed);
            if (!result) {
                size_t size = (index + 1) * ALIGNMENT;
                result = fetchFromPageCache(index);

                if (!result) {
                    _locks[index].clear(std::memory_order_release);
                    return nullptr;
                }

                char* start = static_cast<char*>(result);
                size_t pageNum = (size <= SPAN_PAGES * PAGE_SIZE) ? 
                    SPAN_PAGES : (size + PAGE_SIZE - 1) / PAGE_SIZE;

                size_t blockCnt = (pageNum * PAGE_SIZE) / size;
                if (blockCnt > 1) {
                    for (size_t i = 1; i < blockCnt; i++) {
                        void* current = start + (i - 1) * size;
                        void* next = start + i * size;
                        *reinterpret_cast<void**>(current) = next;
                    }
                    *reinterpret_cast<void**>(start + (blockCnt - 1) * size ) = nullptr;

                    void* next = *reinterpret_cast<void**>(result);
                    *reinterpret_cast<void**>(result) = nullptr;
                    _centralFreeList[index].store(next, std::memory_order_release);

                    size_t trackIndex = _spanCount++;
                    if (trackIndex < _spanTrackers.size()) {
                        _spanTrackers[trackIndex].spanAddress.store(result, std::memory_order_release);
                        _spanTrackers[trackIndex].numPages.store(pageNum, std::memory_order_release);
                        _spanTrackers[trackIndex].blockCount.store(blockCnt, std::memory_order_release);
                        _spanTrackers[trackIndex].freeCount.store(blockCnt - 1, std::memory_order_release);
                    }
                }
            } else {
                void* next = *reinterpret_cast<void**>(result);
                *reinterpret_cast<void**>(result) = nullptr;
                _centralFreeList[index].store(next, std::memory_order_release);

                SpanTracker* tracker = getSpanTracker(result);
                if (tracker) {
                    tracker->freeCount.fetch_sub(1, std::memory_order_release);
                }
            }
        } catch(...) {
            _locks[index].clear(std::memory_order_release);
            throw;
        }

        _locks[index].clear(std::memory_order_release);
        return result;
    }

    void CentralCache::returnRange(void* start, size_t size, size_t index) {
        if (!start || index >= FREE_LIST_SIZE) return;

        size_t blockSize = (index + 1) * ALIGNMENT;
        size_t blockCnt = size / blockSize;

        while (_locks[index].test_and_set(std::memory_order_acquire)) 
            std::this_thread::yield();

        try {
            size_t count = 1;
            void* end = start;
            while (*reinterpret_cast<void**>(end) && count < blockCnt) {
                end = *reinterpret_cast<void**>(end);
                count++;
            }
            void* current = _centralFreeList[index].load(std::memory_order_relaxed);
            *reinterpret_cast<void**>(end) = current;
            _centralFreeList[index].store(start, std::memory_order_release);

            size_t currentCnt = _delayCounts[index].fetch_add(count, std::memory_order_release) + 1;
            auto currentTime = std::chrono::steady_clock::now();

            if (shouldPerformDelayedReturn(index, currentCnt, currentTime)) performDelayedReturn(index);
        } catch(...) {
            _locks[index].clear(std::memory_order_release);
            throw;
        }

        _locks[index].clear(std::memory_order_release);
    }

    bool CentralCache::shouldPerformDelayedReturn(size_t index, size_t currentCnt, std::chrono::steady_clock::time_point curTime) {
        if (currentCnt >= MAX_DELAY_COUNT) return true;

        auto lastTime = _lastReturnTimes[index];
        return (curTime - lastTime) >= DELAY_INTERVAL;
    }

    void CentralCache::performDelayedReturn(size_t index) {
        _delayCounts[index].store(0, std::memory_order_relaxed);
        _lastReturnTimes[index] = std::chrono::steady_clock::now();

        std::unordered_map<SpanTracker*, size_t> spanFreeCounts;
        void* curBlock = _centralFreeList[index].load(std::memory_order_relaxed);

        while (curBlock) {
            SpanTracker* tracker = getSpanTracker(curBlock);
            if (tracker) spanFreeCounts[tracker]++;
            curBlock = *reinterpret_cast<void**>(curBlock);
        }

        for (const auto& [tracker, newFreeBlocks] : spanFreeCounts)
            updateSpanFreeCount(tracker, newFreeBlocks, index);
    }

    void CentralCache::updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index) {
        size_t oldFreeCnt = tracker->freeCount.load(std::memory_order_relaxed);
        size_t newFreeCnt = oldFreeCnt + newFreeBlocks;
        tracker->freeCount.store(newFreeCnt, std::memory_order_release);

        if (newFreeCnt == tracker->blockCount.load(std::memory_order_relaxed)) {
            void* spanAddr = tracker->spanAddress.load(std::memory_order_relaxed);
            size_t pageNum = tracker->numPages.load(std::memory_order_relaxed);

            void* head = _centralFreeList[index].load(std::memory_order_relaxed);
            void* newHead = nullptr;
            void* prev = nullptr;
            void* current = head;

            while (current) {
                void* next = *reinterpret_cast<void**>(current);
                if (current >= spanAddr && current < static_cast<char*>(spanAddr) + pageNum * PAGE_SIZE) {
                    if (prev) *reinterpret_cast<void**>(prev) = next;
                    else newHead = next;
                } else {
                    prev = current;
                }
                current = next;
            }

            _centralFreeList[index].store(newHead, std::memory_order_release);
            PageCache::getInstance().deallocateSpan(spanAddr, pageNum);
        }
    }

    void* CentralCache::fetchFromPageCache(size_t size) {
        size_t pageNum = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        return size <= SPAN_PAGES * PAGE_SIZE ? 
            PageCache::getInstance().allocateSpan(SPAN_PAGES) : 
            PageCache::getInstance().allocateSpan(pageNum);
    }

    SpanTracker* CentralCache::getSpanTracker(void* blockAddress) {
        for (size_t i = 0; i < _spanCount.load(std::memory_order_relaxed); i++) {
            void* spanAddr = _spanTrackers[i].spanAddress.load(std::memory_order_relaxed);
            size_t pageNum = _spanTrackers[i].numPages.load(std::memory_order_relaxed);

            if (blockAddress >= spanAddr && blockAddress < static_cast<char*>(spanAddr) + pageNum * PAGE_SIZE)
                return &_spanTrackers[i];
        }

        return nullptr;
    }
}