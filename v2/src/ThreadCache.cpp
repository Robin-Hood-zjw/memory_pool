#include "./Common.cpp"
#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"

namespace Pool {
    void* ThreadCache::allocate(size_t size) {
        if (size == 0) size = ALIGNMENT;
        if (size > MAX_BYTES) return malloc(size);

        size_t index = SizeClass::getIndex(size);
        void* ptr = _freeList[index];
        _freeListSize[index]--;

        if (ptr) {
            _freeList[index] = *reinterpret_cast<void**>(ptr);
            return ptr;
        }
        return fetchFromCentralCache(index);
    }

    void ThreadCache::deallocate(void* ptr, size_t size) {
        if (size > MAX_BYTES) {
            free(ptr);
            return;
        }

        size_t index = SizeClass::getIndex(size);
        *reinterpret_cast<void**>(ptr) = _freeList[index];
        _freeList[index] = ptr;
        _freeListSize[index]++;

        if (shouldReturnToCentralCache(index)) returnToCentralCache(ptr, size);
    }

    void* ThreadCache::fetchFromCentralCache(size_t index) {
        void* start = CentralCache::getInstance().fetchRange(index);
        if (!start) return nullptr;

        size_t batchNum = 0;
        void* result = start;
        void* current = start;
        _freeList[index] = *reinterpret_cast<void**>(start);

        while (current) {
            current = *reinterpret_cast<void**>(current);
            batchNum++;
        }

        _freeListSize[index] += batchNum;
        return result;
    }

    void ThreadCache::returnToCentralCache(void* start, size_t size) {
        size_t index = SizeClass::getIndex(size);
        size_t batchNum = _freeListSize[index];
        if (batchNum < 2) return;

        size_t keepNum = std::max(batchNum / 4, size_t(1));
        size_t returnNum = batchNum - keepNum;
        char* current = static_cast<char*>(start);
        char* splitNode = current;

        for (size_t i = 0; i < keepNum - 1; i++) {
            void* next = *reinterpret_cast<void**>(current);
            current = reinterpret_cast<char*>(next);

            if (!current) {
                returnNum = batchNum - (i + 1);
                return;
            }
        }

        if (current) {
            void* next = *reinterpret_cast<void**>(current);
            *reinterpret_cast<void**>(current) = nullptr;
            _freeList[index] = start;
            _freeListSize[index] = keepNum;

            if (returnNum > 0 && next) {
                size_t size2 = SizeClass::roundUp(size) * returnNum;
                CentralCache::getInstance().returnRange(start, size2, index);
            }
        }
    }

    bool ThreadCache::shouldReturnToCentralCache(size_t index) {
        size_t threshold = 256;
        return _freeListSize[index] > threshold;
    }
}