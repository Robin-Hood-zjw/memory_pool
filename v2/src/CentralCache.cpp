#include "../include/PageCache.h"
#include "../include/CentralCache.h"

namespace Pool {
    const std::chrono::milliseconds CentralCache::DELAY_INTERVAL{1000};

    /**
     * @brief constructor: initializes atomic locks, resets delay counters, 
     *        sets the baseline time points for all size buckets
     **/
    CentralCache::CentralCache() {
        // initialize spinlocks to clear state (unlocked)
        for (auto& lock : _locks) lock.clear();
        // reset all central free list heads using relaxed memory order
        for (auto& ptr : _centralFreeList) ptr.store(nullptr, std::memory_order_relaxed);
        // reset delayed return counters with 0
        for (auto& count : _delayCounts) count.store(0, std::memory_order_relaxed);
        // seed the time points to current time to prevent immediate firing
        for (auto& time : _lastReturnTimes) time = std::chrono::steady_clock::now();
        // start span registration counter at 0
        _spanCount.store(0, std::memory_order_relaxed);
    };

    /**
     * @brief decide if to trigger a bulk return to PageCache
     * @param index the current index of size class
     * @param currentCnt accumulated count of returned blocks since last sync
     * @param curTime the current timestamp
     * @return True if the threshold (count or time) is met
     **/
    bool CentralCache::shouldPerformDelayedReturn(size_t index, size_t currentCnt, std::chrono::steady_clock::time_point curTime) {
        // condition 1: accumulated block count exceeds the high-water mark
        if (currentCnt >= MAX_DELAY_COUNT) return true;
        // condition 2: time elapsed since last return exceeds the cool-down interval
        auto lastTime = _lastReturnTimes[index];
        return (curTime - lastTime) >= DELAY_INTERVAL;
    }

    /**
     * @brief scan the local free list and updates global span metadata
     * @param index the size class bucket to process
     **/
    void CentralCache::performDelayedReturn(size_t index) {
        // reset metrics(_delayCounts & _lastReturnTimes) for this bucket
        _delayCounts[index].store(0, std::memory_order_relaxed);
        _lastReturnTimes[index] = std::chrono::steady_clock::now();

        // count how many free blocks belong to each Span
        std::unordered_map<SpanTracker*, size_t> freeSpanCnt;
        void* cur = _centralFreeList[index].load(std::memory_order_relaxed);
        while (cur) {
            SpanTracker* tracker = getSpanTracker(cur);
            if (tracker) freeSpanCnt[tracker]++;
            cur = *reinterpret_cast<void**>(cur);
        }

        // update each affected Span's state
        for (const auto& [tracker, count] : freeSpanCnt) updateSpanFreeCount(tracker, count, index);
    }

    /**
     * @brief increments the free block count of a span; 
     *        unlinks all blocks from the central list;
     *        returns it to PageCache if the empty span
     * @param tracker a pointer to the metadata tracker for the specific span
     * @param newFreeBlocks the number of newly freed blocks belonging to this span
     * @param index the size bucket index for list manipulation
     **/
    void CentralCache::updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index) {
        // atomic update of the free counter
        size_t oldFreeCnt = tracker->freeCount.load(std::memory_order_relaxed);
        size_t newFreeCnt = oldFreeCnt + newFreeBlocks;
        tracker->freeCount.store(newFreeCnt, std::memory_order_release);

        // check if the span is now 100% empty
        if (newFreeCnt == tracker->blockCount.load(std::memory_order_relaxed)) {
            // calculate the boundry of the span
            void* start = tracker->spanAddress.load(std::memory_order_relaxed);
            size_t pageNum = tracker->numPages.load(std::memory_order_relaxed);
            void* end = static_cast<char*>(start) + pageNum * PAGE_SIZE;

            // remove all blocks in this span from the central list
            void* cur = _centralFreeList[index].load(std::memory_order_relaxed);
            void* prev = nullptr;
            void* newHead = nullptr;

            while (cur) {
                void* next = *reinterpret_cast<void**>(cur);
                if (cur >= start && cur < end) {
                    if (prev) *reinterpret_cast<void**>(prev) = next;
                    else newHead = next;
                } else {
                    prev = cur;
                }
                cur = next;
            }

            // update the list head and return memory back to PageCache
            _centralFreeList[index].store(newHead, std::memory_order_release);
            PageCache::getInstance().deallocateSpan(start, pageNum);
        }
    }

    /**
     * @brief decides the page number to request from PageCache based on object size
     * @param size the object size in bytes
     * @return void* a pointer to the allocated Span memory
     **/
    void* CentralCache::fetchFromPageCache(size_t size) {
        // calculate the page number for one object
        size_t pageNum = (size + PAGE_SIZE - 1) / PAGE_SIZE;

        // request a standard large SPAN to split if small size
        // request exact fit if larger than the standard size
        return size <= SPAN_PAGES * PAGE_SIZE ?
            PageCache::getInstance().allocateSpan(SPAN_PAGES):
            PageCache::getInstance().allocateSpan(pageNum);
    }

    /**
     * @brief performs a linear search to find which SpanTracker manages a specific memory address
     * @param blockAddress the memory pointer to look up
     * @return SpanTracker* the matching tracker or nullptr if not found
     **/
    SpanTracker* CentralCache::getSpanTracker(void* blockAddress) {
        for (size_t i = 0; i < _spanCount.load(std::memory_order_relaxed); i++) {
            void* start = _spanTrackers[i].spanAddress.load(std::memory_order_relaxed);
            size_t pageNum = _spanTrackers[i].numPages.load(std::memory_order_relaxed);
            void* end = static_cast<char*>(start) + pageNum * PAGE_SIZE;

            if (blockAddress >= start && blockAddress < end) return &_spanTrackers[i];
        }
        return nullptr;
    }

    /**
     * @brief Thread-safe singleton accessor for the Central Cache.
     * @return CentralCache& a reference to the single global instance
     **/
    CentralCache& CentralCache::getInstance() {
        static CentralCache instance;
        return instance;
    }

    /**
     * @brief offer a single memory block to a ThreadCache;
     *        if the bucket is empty, fetch a new span from PageCache & carves it into blocks
     * @param index the size class index
     * @return void* a pointer to one usable memory block
     **/
    void* CentralCache::fetchRange(size_t index) {
        // return NULL if larger the max size
        if (index >= FREE_LIST_SIZE) return nullptr;

        // acquire fine-grained bucket lock (spin-yield pattern)
        while (_locks[index].test_and_set(std::memory_order_acquire)) std::this_thread::yield();

        void* result = nullptr;
        try {
            result = _centralFreeList[index].load(std::memory_order_relaxed);

            if (!result) {
                // fetch from PageCache if no left blocks in the central list
                size_t size = (index + 1) * ALIGNMENT;
                result = fetchFromPageCache(size);

                // return NULL if fail to fetch from PageCache
                if (!result) {
                    _locks[index].clear();
                    return nullptr;
                }

                // divide the large span into small blocks
                char* start = static_cast<char*>(result);
                size_t pageNum = size <= SPAN_PAGES * PAGE_SIZE ? 
                    SPAN_PAGES : (size + PAGE_SIZE - 1) / PAGE_SIZE;
                size_t blockCnt = pageNum * PAGE_SIZE / size;

                if (blockCnt > 1) {
                    // link blocks together and set the tail for this linked list
                    for (size_t i = 1; i < blockCnt; i++) {
                        void* cur = start + (i - 1) * size;
                        void* next = start + i * size;
                        *reinterpret_cast<void**>(cur) = next;
                    }
                    *reinterpret_cast<void**>(start + (blockCnt - 1) * size) = nullptr;

                    // pop a block from the central list
                    void* next = *reinterpret_cast<void**>(result);
                    *reinterpret_cast<void**>(result) = nullptr;
                    _centralFreeList[index].store(next, std::memory_order_release);

                    // register the info of the taregt span tracker
                    size_t idx = _spanCount;
                    if (idx < _spanTrackers.size()) {
                        _spanTrackers[idx].spanAddress.store(result, std::memory_order_release);
                        _spanTrackers[idx].numPages.store(pageNum, std::memory_order_release);
                        _spanTrackers[idx].freeCount.store(blockCnt, std::memory_order_release);
                        _spanTrackers[idx].blockCount.store(blockCnt - 1, std::memory_order_release);
                    }
                    _spanCount++;
                }
            } else {
                // pop a block from the central list
                void* next = *reinterpret_cast<void**>(result);
                *reinterpret_cast<void**>(result) = nullptr;
                _centralFreeList[index].store(next, std::memory_order_release);

                // update the count of free blocks in the related span tracker
                SpanTracker* tracker = getSpanTracker(result);
                if (tracker) tracker->freeCount.fetch_sub(1, std::memory_order_release);
            }
        } catch(...) {
            _locks[index].clear(std::memory_order_release);
            throw;
        }

        _locks[index].clear(std::memory_order_release);
        return result;
    }

    /**
     * @brief receive memory blocks from ThreadCache & adds them to the central free list;
     *        triggers a delayed return check
     * @param start the head of the returned block list
     * @param size the total size of the returned batch
     * @param index the size class index
     **/
    void CentralCache::returnRange(void* start, size_t size, size_t index) {
        // return NULL if no start or over the max size
        if (!start || index > FREE_LIST_SIZE) return;

        // calculate the block count
        size_t blockSize = (index + 1) * ALIGNMENT;
        size_t blockCnt = size / blockSize;

        // acquire fine-grained bucket lock (spin-yield pattern)
        while (_locks[index].test_and_set(std::memory_order_release)) std::this_thread::yield();

        try {
            // find the tail of the batch
            size_t count = 0;
            void* end = start;
            while (*reinterpret_cast<void**>(end) && count < blockCnt) {
                end = *reinterpret_cast<void**>(end);
                count++;
            }

            // link the tail of the batch to the current list
            void* current = _centralFreeList[index].load(std::memory_order_relaxed);
            *reinterpret_cast<void**>(end) = current;
            _centralFreeList[index].store(start, std::memory_order_release);

            // decide if reclaimation should be performed
            size_t curCnt = _delayCounts[index].fetch_add(blockCnt, std::memory_order_release);
            auto curTime = std::chrono::steady_clock::now();
            if (shouldPerformDelayedReturn(index, curCnt, curTime)) performDelayedReturn(index);
        } catch (...) {
            _locks[index].clear(std::memory_order_release);
            throw;
        }

        _locks[index].clear(std::memory_order_release);
    }
}