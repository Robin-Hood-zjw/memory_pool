#include "Timer.cpp"
#include "../include/MemoryPool.h"

#include <array>
#include <chrono>
#include <vector>
#include <random>
#include <thread>
#include <iomanip>
#include <iostream>

using namespace Pool;

class PerformanceTest {
private:
    struct TestStats {
        size_t totalBytes = 0;
        size_t totalAllocs = 0;
        double systemTime = 0.0;
        double memPoolTime = 0.0;
    };

public:
    /**
     * @brief pre-heat the memory system
     * This is crucial because the first time you call mmap or allocate static memory, 
     * the OS has to handle page faults and the CPU has to populate caches.
     **/
    static void warmup() {
        std::cout << "Warming up memory systems..." << std::endl;
        std::vector<std::pair<void*, size_t>> warmupPtrs;

        // allocate 1000 instances of each size class
        for (size_t i = 0; i < 1000; ++i) {
            for (size_t size : {8, 16, 32, 64, 128, 256, 512, 1024}) {
                void* p = MemoryPool::allocate(size);
                warmupPtrs.emplace_back(p, size);
            }
        }

        // deallocates them to populate the ThreadCache and CentralCache
        for (const auto& [ptr, size] : warmupPtrs)
            MemoryPool::deallocate(ptr, size);

        std::cout << "Warmup complete.\n" << std::endl;
    }

    /**
     * @brief Measures throughput for high-frequency small object allocation.
     * 1. Defines a test set of small sizes (8B to 256B).
     * 2. Allocates 50,000 objects in a loop.
     * 3. Randomly deallocates objects (25% frequency) to simulate memory reuse.
     * 4. Compares the time taken by MemoryPool vs standard new/delete.
     **/
    static void testSmallAllocation() {
        constexpr size_t NUM_ALLOCS = 50000;                        // the number of executed application operation in total
        const size_t SIZES[] = {8, 16, 32, 64, 128, 256};           // define 6 different specifications for small objects
        const size_t NUM_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);  // calculate the number of size classes

        std::cout<< "\nTesting small allocations (" << NUM_ALLOCS << " allocations of fixed sizes):" << std::endl;

        // MemoryPool Block Procedures: 
        // start Timer -> allocate objects -> deallocate intermittently -> clean up remaining -> print elapsed time
        {
            Timer t;                                                                // timing begins upon construction
            std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;  // an array of vectors to hold allocated pointers for each size class
            for (auto& ptrs : sizePtrs) ptrs.reserve(NUM_ALLOCS / NUM_SIZES);       // reserve space to avoid reallocations during push_back

            // allocate NUM_ALLOCS objects, cycling through the defined sizes
            for (size_t i = 0; i < NUM_ALLOCS; i++) {
                size_t size = SIZES[i % NUM_SIZES];             // select size based on current index to ensure a mix of sizes
                void* p = MemoryPool::allocate(size);           // allocate memory using MemoryPool
                sizePtrs[i % NUM_SIZES].emplace_back(p, size);  // store the pointer and its size for later deallocation

                // randomly deallocate pointers to simulate a workload where not all allocations are freed immediately
                // test the allocator's ability to handle fragmentation and reuse memory
                if (i % 4 == 0) {
                    size_t releaseIdx = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIdx];

                    if (!ptrs.empty()) {
                        auto [ptr, size] = ptrs.back();
                        MemoryPool::deallocate(ptr, size);
                        ptrs.pop_back();
                    }
                }
            }

            // deallocate any remaining pointers to ensure no leaking memory
            // test the allocator's performance when freeing a large number of objects at once
            for (auto& ptrs : sizePtrs) {
                for (const auto& [ptr, size] : ptrs) MemoryPool::deallocate(ptr, size);
            }

            std::cout << "MemoryPool: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
        }

        // Standard Block Procedures:
        // start Timer -> allocate objects -> deallocate intermittently -> clean up remaining -> print elapsed time
        {
            Timer t;                                                                    // timing begins upon construction
            std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;      // an array of vectors to hold allocated pointers for each size class
            for (auto& ptrs : sizePtrs) ptrs.reserve(NUM_ALLOCS / NUM_SIZES);           // reserve space to avoid reallocations during push_back

            // allocate NUM_ALLOCS objects, cycling through the defined sizes
            for (size_t i = 0; i < NUM_ALLOCS; i++) {
                size_t size = SIZES[i % NUM_SIZES];
                void* ptr = new char[size];
                sizePtrs[i % NUM_SIZES].push_back({ptr, size});

                // randomly deallocate pointers to simulate a workload where not all allocations are freed immediately
                if (i % 4 == 0) {
                    size_t releaseIdx = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIdx];

                    if (!ptrs.empty()) {
                        auto [ptr, size] = ptrs.back();
                        delete[] static_cast<char*>(ptr);
                        ptrs.pop_back();
                    }
                }
            }

            // deallocate any remaining pointers to ensure no leaking memory
            for (auto& ptrs : sizePtrs) {
                for (const auto& [ptr, size] : ptrs) delete[] static_cast<char*>(ptr);
            }

            std::cout << "New/Delete: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
    }
    }

    /**
     * @brief simulate a high-concurrency environment to test ThreadCache lock-free paths
     **/
    static void testMultiThreaded() {
            constexpr size_t NUM_THREADS = 4;               // number of concurrent threads to simulate
            constexpr size_t ALLOCS_PER_THREAD = 25000;     // number of allocations each thread will perform, leading to a total of 100,000 allocations across all threads
            
            std::cout << "\nTesting multi-threaded allocations (" << NUM_THREADS << " threads, " << ALLOCS_PER_THREAD << " allocations each):" << std::endl;

            // each thread will perform a mix of allocations and deallocations, with random sizes and intermittent releases
            auto threadFunc = [](bool useMemPool) {
                std::random_device rd;                                                      // seed for random number generator
                std::mt19937 gen(rd());                                                     // Mersenne Twister random number generator

                const size_t SIZES[] = {8, 16, 32, 64, 128, 256};                           // a set of sizes to allocate
                const size_t NUM_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);                  // calculate the number of size classes for indexing
                std::array<std::vector<std::pair<void*, size_t>>, NUM_SIZES> sizePtrs;      // an array of vectors to hold allocated pointers for each size class

                // reserve space in each vector to minimize reallocations during push_back, improving performance and simulating a realistic workload where many allocations of each size occur
                for (auto& ptrs : sizePtrs) ptrs.reserve(ALLOCS_PER_THREAD / NUM_SIZES);

                // each thread performs a series of allocations, randomly deallocating some to simulate a realistic workload where not all memory is freed immediately
                for (size_t i = 0; i < ALLOCS_PER_THREAD; i++) {
                    size_t size = SIZES[i % NUM_SIZES];
                    void* ptr = useMemPool ? MemoryPool::allocate(size) : new char[size];
                    sizePtrs[i % NUM_SIZES].push_back({ptr, size});

                    // randomly deallocate some pointers to simulate a workload where not all allocations are freed immediately, testing the allocator's ability to handle fragmentation and reuse memory under concurrent conditions
                    if (i % 100 == 0) {
                        auto& ptrs = sizePtrs[rand() % NUM_SIZES];                             // randomly select a size class to deallocate from
                        
                        if (!ptrs.empty()) {
                            size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;    // randomly determine how many pointers to release (between 20% and 30% of the current pointers)
                            releaseCount = std::min(releaseCount, ptrs.size());                // ensure we don't try to release more pointers than we have

                            // randomly release pointers from the selected size class, testing the allocator's performance and correctness when multiple threads are allocating and deallocating memory concurrently, which can lead to contention and fragmentation
                            for (size_t j = 0; j < releaseCount; j++) {
                                size_t index = rand() % ptrs.size();
                                if (useMemPool)  {
                                    MemoryPool::deallocate(ptrs[index].first, ptrs[index].second);
                                } else {
                                    delete[] static_cast<char*>(ptrs[index].first);
                                }

                                // remove the released pointer from the vector by swapping it with the last element and popping back
                                ptrs[index] = ptrs.back();
                                ptrs.pop_back();
                            }
                        }
                    }

                    // introduce additional random pressure on the allocator every 1000 iterations to simulate a more realistic workload and test the allocator's performance under stress, ensuring it can handle bursts of allocations and deallocations without significant degradation in performance
                    if (i % 1000 == 0) {
                        std::vector<std::pair<void*, size_t>> pressurePtrs;
                        for (int j = 0; j < 50; j++) {
                            size_t size = SIZES[rand() % NUM_SIZES];
                            void* ptr = useMemPool ? MemoryPool::allocate(size) : new char[size];
                            pressurePtrs.push_back({ptr, size});
                        }

                        for (const auto& [ptr, size] : pressurePtrs) {
                            if (useMemPool) {
                                MemoryPool::deallocate(ptr, size);
                            } else {
                                delete[] static_cast<char*>(ptr);
                            }
                        }
                    }
                }

                for (auto& ptrs : sizePtrs) {
                    for (const auto& [ptr, size] : ptrs)  {
                        if (useMemPool)  {
                            MemoryPool::deallocate(ptr, size);
                        } else {
                            delete[] static_cast<char*>(ptr);
                        }
                    }
                }
            };

            // launch threads for MemoryPool test and measure elapsed time
            {
                Timer t;
                std::vector<std::thread> threads;
                
                for (size_t i = 0; i < NUM_THREADS; ++i)
                    threads.emplace_back(threadFunc, true);
                for (auto& thread : threads) thread.join();

                std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
            }

            // launch threads for standard new/delete test and measure elapsed time
            {
                Timer t;
                std::vector<std::thread> threads;

                for (size_t i = 0; i < NUM_THREADS; ++i) 
                    threads.emplace_back(threadFunc, false);
                for (auto& thread : threads) thread.join();

                std::cout << "New/Delete: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
            }
        }

    /**
     * @brief test non-uniform allocation patterns (60% small, 30% medium, 10% large)
     **/
    static void testMixedSizes() {
            constexpr size_t NUM_ALLOCS = 100000;

            const size_t S_SIZES[] = {8, 16, 32, 64, 128};
            const size_t M_SIZES[] = {256, 384, 512};
            const size_t L_SIZES[] = {1024, 2048, 4096};

            const size_t NUM_S = sizeof(S_SIZES) / sizeof(S_SIZES[0]);
            const size_t NUM_M = sizeof(M_SIZES) / sizeof(M_SIZES[0]);
            const size_t NUM_L = sizeof(L_SIZES) / sizeof(L_SIZES[0]);
            const size_t NUM_TOTAL_SIZES = NUM_S + NUM_M + NUM_L;

            std::cout << "\nTesting mixed size allocations (" << NUM_ALLOCS << " allocations with fixed sizes):" << std::endl;

            // test block 1: custom MemoryPool
            {
                Timer t;
                std::array<std::vector<std::pair<void*, size_t>>, NUM_TOTAL_SIZES> sizePtrs; // an array of vectors to hold allocated pointers for each size class
                for (auto& ptrs : sizePtrs) ptrs.reserve(NUM_ALLOCS / NUM_TOTAL_SIZES);      // reserve space to avoid reallocations during push_back

                // test the allocator's performance under conditions where certain size classes are more common
                // evaluate how well the allocator handles fragmentation and reuse of memory across different size classes
                for (size_t i = 0; i < NUM_ALLOCS; ++i) {
                    size_t size, ptrIndex;
                    int category = i % 100;

                    // step 1: assign size based on probability tiers (60% small, 30% medium, 10% large)
                    if (category < 60) {
                        ptrIndex = (i / 60) % NUM_S;
                        size = S_SIZES[ptrIndex];
                    } else if (category < 90) {
                        ptrIndex = (i / 30) % NUM_M;
                        size = M_SIZES[ptrIndex];
                    } else {
                        ptrIndex = (i / 10) % NUM_L;
                        size = L_SIZES[ptrIndex];
                    }

                    // step 2: perform allocation
                    void* ptr = MemoryPool::allocate(size);
                    // step 3: map category back to the correct vector index for storage
                    sizePtrs[ptrIndex].push_back({ptr, size});

                    // step 4: batch release (intermittent deallocation)
                    // release 20-30% of a random size class's memory every 50 iterations
                    if (i % 50 == 0) {
                        size_t releaseIndex = rand() % sizePtrs.size();
                        auto& ptrs = sizePtrs[releaseIndex];
                        
                        if (!ptrs.empty()) {
                            size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;
                            releaseCount = std::min(releaseCount, ptrs.size());
                            
                            for (size_t j = 0; j < releaseCount; j++) {
                                size_t index = rand() % ptrs.size();
                                MemoryPool::deallocate(ptrs[index].first, ptrs[index].second);
                                ptrs[index] = ptrs.back();
                                ptrs.pop_back();
                            }
                        }
                    }
                }

                // final cleanup: return all remaining blocks to the pool
                for (auto& ptrs : sizePtrs) {
                    for (const auto& [ptr, size] : ptrs) MemoryPool::deallocate(ptr, size);
                }
                
                std::cout << "Memory Pool: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
            }

            // TEST BLOCK 2: Standard new/delete
            {
                Timer t;
                std::array<std::vector<std::pair<void*, size_t>>, NUM_TOTAL_SIZES> sizePtrs;    // an array of vectors to hold allocated pointers for each size class
                for (auto& ptrs : sizePtrs) ptrs.reserve(NUM_ALLOCS / NUM_TOTAL_SIZES);         // reserve space to avoid reallocations during push_back

                for (size_t i = 0; i < NUM_ALLOCS; i++) {
                    size_t size, ptrIndex;
                    int category = i % 100;

                    // step 1: assign size based on probability tiers (60% small, 30% medium, 10% large)
                    if (category < 60) {
                        ptrIndex = (i / 60) % NUM_S;
                        size = S_SIZES[ptrIndex];
                    } else if (category < 90) {
                        ptrIndex = (i / 30) % NUM_M;
                        size = M_SIZES[ptrIndex];
                    } else {
                        ptrIndex = (i / 10) % NUM_L;
                        size = L_SIZES[ptrIndex];
                    }

                    // step 2: perform allocation
                    void* ptr = new char[size];
                    // step 3: map category back to the correct vector index for storage
                    sizePtrs[ptrIndex].push_back({ptr, size});

                    // step 4: batch release (intermittent deallocation)
                    if (i % 50 == 0) {
                        size_t releaseIndex = rand() % sizePtrs.size();
                        auto& ptrs = sizePtrs[releaseIndex];

                        if (!ptrs.empty()) {
                            size_t releaseCount = ptrs.size() * (20 + (rand() % 11)) / 100;
                            releaseCount = std::min(releaseCount, ptrs.size());

                            for (size_t j = 0; j < releaseCount; ++j) {
                                size_t index = rand() % ptrs.size();
                                delete[] static_cast<char*>(ptrs[index].first);
                                ptrs[index] = ptrs.back();
                                ptrs.pop_back();
                            }
                        }
                    }
                }

                // final cleanup: delete all remaining blocks
                for (auto& ptrs : sizePtrs) {
                    for (const auto& [ptr, size] : ptrs) delete[] static_cast<char*>(ptr);
                }
                
                std::cout << "New/Delete: " << std::fixed << std::setprecision(3) << t.elapsed() << " ms" << std::endl;
            }
        }
};

int main() {
    std::cout << "Starting performance tests..." << std::endl;

    PerformanceTest::warmup();
    PerformanceTest::testSmallAllocation();
    PerformanceTest::testMultiThreaded();
    PerformanceTest::testMixedSizes();

    return 0;
}