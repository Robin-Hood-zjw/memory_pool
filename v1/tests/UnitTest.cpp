#include <vector>
#include <thread>
#include <iostream>

#include "../include/MemoryPool.h"
#include "../include/HashBucket.h"

using namespace Pool;

class P1 { int _id; };
class P2 { int _id[5]; };
class P3 { int _id[10]; };
class P4 { int _id[20]; };

// scenario: highly concurrent and competitive environment
// purpose: measure the performance of the MemoryPool system when performing the operations of object allocation and deallocation
void Benchmark1(size_t ntimes, size_t nworks, size_t rounds) {
    size_t total_cost_time = 0;
    std::vector<std::thread> vthread(nworks);

    // create and start a specified number of concurrent worker threads
    for (size_t k = 0; k < nworks; k++) {
        vthread[k] = std::thread([&](){
            for (size_t j = 0; j < rounds; j++) {
                // record the processor clock at the start of each round
                size_t begin1 = clock();

                for (size_t i = 0; i < ntimes; i++) {
                    // intensive testing: simultaneously allocate and immediately release 4 different object sizes
                    // frequently trigger HashBucket's routing logic and MemoryPool's atomic/lock contention
                    P1* p1 = newElement<P1>();
                    deleteElement<P1>(p1);
                    P2* p2 = newElement<P2>();
                    deleteElement<P2>(p2);
                    P3* p3 = newElement<P3>();
                    deleteElement<P3>(p3);
                    P4* p4 = newElement<P4>();
                    deleteElement<P4>(p4);
                }

                size_t end1 = clock();
                total_cost_time += end1 - begin1;
            }
        });
    }

    // wait for all background threads to finish executing
    for (auto& t : vthread) t.join();
    printf("================================================\n");
    printf("Number of test threads: %lu\n", nworks);
    printf("Per thread round: %lu\n", rounds);
    printf("Number of allocations per round: %lu (x 4 types)\n", ntimes);
    printf("Total CPU time: %lu ms\n", total_cost_time);
    printf("================================================\n");
}

void Benchmark2(size_t ntimes, size_t nworks, size_t rounds) {
	size_t total_cost_time = 0;
	std::vector<std::thread> vthread(nworks);

    // create and start a specified number of concurrent worker threads
	for (size_t k = 0; k < nworks; k++) {
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; j++) {
                // record the processor clock at the start of each round
				size_t begin1 = clock();

				for (size_t i = 0; i < ntimes; i++) {
                    // call the C++ standard operators new and delete
                    // triggers the underlying malloc/free, involving the operating system's heap management logic
                    P1* p1 = new P1;
                    delete p1;
                    P2* p2 = new P2;
                    delete p2;
                    P3* p3 = new P3;
                    delete p3;
                    P4* p4 = new P4;
                    delete p4;
				}

				size_t end1 = clock();
				total_cost_time += end1 - begin1;
			}
		});
	}

    // wait for all background threads to finish executing
	for (auto& t : vthread) t.join();
    printf("================================================\n");
    printf("Number of test threads: %lu\n", nworks);
    printf("Per thread round: %lu\n", rounds);
    printf("Number of allocations per round: %lu (x 4 types)\n", ntimes);
    printf("Total CPU time: %lu ms\n", total_cost_time);
    printf("================================================\n");
}

int main() {
    Pool::HashBucket::initMemoryPool();
    Benchmark1(100, 1, 10);
	std::cout << "===========================================================================" << std::endl;
	std::cout << "===========================================================================" << std::endl;
    Benchmark2(100, 1, 10);
    return 0;
}