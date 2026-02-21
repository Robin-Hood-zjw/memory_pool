#include <vector>
#include <thread>
#include <iostream>

#include "./src/MemoryPool.h"

using namespace MemoryPool;

class P1 { int _id; };
class P2 { int _id[5]; };
class P3 { int _id[10]; };
class P4 { int _id[20]; };

void BenchmarkMemoryPool(size_t ntimes, size_t nworks, size_t rounds) {
    size_t total_cost_time = 0;
    std::vector<std::thread> vthread(nworks);

    for (size_t k = 0; k < nworks; k++) {
        vthread[k] = std::thread([&](){
            for (size_t j = 0; j < rounds; j++) {
                size_t begin1 = clock();
                
            }
        });
    }
    
}