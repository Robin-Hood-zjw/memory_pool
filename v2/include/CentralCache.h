#pragma once

#include "./Common.h"

#include <array>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace Pool {
    /**
     * @brief tracks the metadata of a large memory span
     * determine when a span is fully empty or ready to be returned to PageCache
     **/
    struct SpanTracker {
        std::atomic<void*> spanAddress = nullptr;                                                                           // the address of the span
        std::atomic<size_t> pageNum = 0;                                                                                   // total pages of the span
        std::atomic<size_t> blockCount = 0;                                                                                 // the number of total small blocks in this span
        std::atomic<size_t> freeCount = 0;                                                                                  // the number of available small blocks
    };

    /**
     * @brief coordinates between ThreadCache and PageCache
     **/
    class CentralCache {
    private:
        std::array<std::atomic<void*>, FREE_LIST_SIZE> _centralFreeList;                                                    // an array of linked lists for each size class
        std::array<std::atomic_flag, FREE_LIST_SIZE> _locks;                                                                // fine-grained locks: one lock per size bucket to minimize contention
        std::array<SpanTracker, 1024> _spanTrackers;                                                                        // Metadata storage for active spans
        std::atomic<size_t> _spanCount = 0;                                                                                 // a global counter that manages the allocation and tracking of the `_spanTrackers` array

        std::array<std::atomic<size_t>, FREE_LIST_SIZE> _delayCounts;                                                       // an counter array to record the number of free blocks received by this Bucket since the last reclamation
        std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> _lastReturnTimes;                                 // an timestamp array to record the timestamp when each size class's bucket last executed the "return to PageCache"
        static const std::chrono::milliseconds DELAY_INTERVAL;

        CentralCache();                                                                                                     // constructor: initialize all the attributes
        void* fetchFromPageCache(size_t size);                                                                              // request a new span from PageCache and cuts it into small blocks
        SpanTracker* getSpanTracker(void* blockAddress);                                                                    // decide if it's time to return memory to PageCache
        void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);                                 // determine if all small blocks in a span have been recycled, and reclaim large block if so 
        bool shouldPerformDelayedReturn(size_t index, size_t currentCnt, std::chrono::steady_clock::time_point curTime);    // determine if the idle blocks in CentralCache have reached a threshold to return to PageCache
        void performDelayedReturn(size_t index);                                                                            // reorganize scattered memory fragments and return them to PageCache in larger blocks
    public:
        static CentralCache& getInstance();                                                                                 // singleton instance: global coordinate the process

        void* fetchRange(size_t index);                                                                                     // offer a batch of blocks to a requesting ThreadCache
        void returnRange(void* start, size_t size, size_t index);                                                           // accept a batch of blocks returned by a ThreadCache
    };
}