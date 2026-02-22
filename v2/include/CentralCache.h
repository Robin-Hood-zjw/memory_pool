#pragma once

#include "./Common.h"

#include <array>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace Pool {
    struct SpanTracker {
        std::atomic<void*> spanAddress = nullptr;
        std::atomic<size_t> numPages = 0;
        std::atomic<size_t> blockCount = 0;
        std::atomic<size_t> freeCount = 0;
    };

    class CentralCache {
        private:
            std::array<std::atomic<void*>, FREE_LIST_SIZE> _centralFreeList;
            std::array<std::atomic_flag, FREE_LIST_SIZE> _locks;
            std::array<SpanTracker, 1024> _spanTrackers;
            std::atomic<size_t> _spanCount = 0;

            static const size_t MAX_DELAY_COUNT = 48;
            std::array<std::atomic<size_t>, FREE_LIST_SIZE> _delayCounts;
            std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> _lastReturnTimes;
            static const std::chrono::milliseconds DELAY_INTERVAL;

            CentralCache();
            void* fetchFromPageCache(size_t size);
            SpanTracker* getSpanTracker(void* blockAddress);
            void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);

            bool shouldPerformDelayedReturn(size_t index, size_t currentCnt, std::chrono::steady_clock::time_point curTime);
            void performDelayedReturn(size_t index);
        public:
            static CentralCache& getInstance() {
                static CentralCache instance;
                return instance;
            }

            void* fetchRange(size_t index);
            void returnRange(void* start, size_t size, size_t index);
    };
}