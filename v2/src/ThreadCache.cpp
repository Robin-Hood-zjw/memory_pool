#include "./Common.cpp"
#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace Pool {
    /**
     * @brief constructor: initializes the per-thread cache.
     * sets all free list heads to nullptr and counters to zero.
     **/
    ThreadCache::ThreadCache() {
        _freeList.fill(nullptr);
        _freeListSize.fill(0);
    }

    /**
     * @brief fetches a batch of memory blocks from CentralCache
     * @param index in the free list array corresponding to the object size
     * @return void* a pointer to the first usable memory block.
     **/
    void* ThreadCache::fetchFromCentralCache(size_t index) {
        // attempt to "wholesale" a range of blocks from CentralCache
        void* start = CentralCache::getInstance().fetchRange(index);
        if (!start) return nullptr;

        // update the head of target free list since the old head is given to a user
        _freeList[index] = *reinterpret_cast<void**>(start);

        // 3. Traverse the linked list to count how many blocks we actually received
        size_t num = 0;
        void* current  = start;

        while (current) {
            current = *reinterpret_cast<void**>(current);
            num++;
        }

        // update the count of free blocks in the target free list
        _freeListSize[index] += num;

        return start;
    }

    /**
     * @brief returns a portion of the local cache back to CentralCache
     * @param start the head of the local free list
     * @param size  the size of the objects in this list
     **/
    void ThreadCache::returnToCentralCache(void* start, size_t size) {
        size_t index = SizeClass::getIndex(size);
        size_t num = _freeListSize[index];
        // safety check: no return if we have nothing (< 2)
        if (num < 2) return;

        // return 25% of the free blocks to CentralCache, or at least 1
        size_t returnNum = std::max(size_t(1), num / 4);
        size_t keepNum = num - returnNum;

        // traverse the free list to find the cut point (the end of the blocks we keep)
        void* current = start;
        for (size_t i = 0; i < keepNum - 1; i++) {
            current = *reinterpret_cast<void**>(current);

            // safeguard: prevent unexpected cut point
            if (!current) {
                returnNum = num - (i + 1);
                return;
            }
        }

        // cut the next connection from the cut point, and return the following part
        if (current) {
            // cut the cut point attached after it
            void* next = *reinterpret_cast<void**>(current);
            *reinterpret_cast<void**>(current) = nullptr;

            // update the head of the target free list and the num of the count list
            _freeList[index] = start;
            _freeListSize[index] = keepNum;

            // cut the following points after the cut point and return them to CentralCache
            if (returnNum > 0 && next) {
                size_t bytes = returnNum * SizeClass::roundUp(size);
                CentralCache::getInstance().returnRange(next, bytes, index);
            }
        }
    }

    /**
     * @brief check to see if local cache for a specific size is too large
     * @return true if the count exceeds a threshold (currently 256)
     **/
    bool ThreadCache::shouldReturnToCentralCache(size_t index) {
        return _freeListSize[index] > size_t(256);
    }

    /**
     * @brief singleton accessor using Thread Local Storage
     * ensures each thread has one private, lock-free ThreadCache instance
     **/
    ThreadCache* ThreadCache::getInstance() {
        static thread_local ThreadCache instance;
        return &instance;
    }

    /**
     * @brief allocates memory for a given size
     * @return a pointer to the allocated memory, or result of malloc if too large size
     **/
    void* ThreadCache::allocate(size_t size) {
        if (size == 0) size = ALIGNMENT;
        if (size > MAX_BYTES) return malloc(size);

        // locate the correct free list for this size
        size_t index = SizeClass::getIndex(size);
        void* ptr = _freeList[index];

        // pop a cached block from the free list and return it
        if (ptr) {
            _freeListSize[index]--;
            _freeList[index] = *reinterpret_cast<void**>(ptr);
            return ptr;
        }

        // request more blocks from CentralCache
        return fetchFromCentralCache(index);
    }

    /**
     * @brief deallocates a memory block
     * @param ptr a pointer to the memory being freed
     * @param size size requested for this block
     **/
    void ThreadCache::deallocate(void* ptr, size_t size) {
        // safeguard: free() large blocks bypass the pool
        if (size > MAX_BYTES) {
            free(ptr);
            return;
        }

        // update the head of the free list and the size of the free list's count
        size_t index = SizeClass::getIndex(size);
        *reinterpret_cast<void**>(ptr) = _freeList[index];
        _freeList[index] = ptr;
        _freeListSize[index]++;

        // return the blocks to CentralCache if local cache is too large
        if (shouldReturnToCentralCache(index)) returnToCentralCache(_freeList[index], size);
    }
}