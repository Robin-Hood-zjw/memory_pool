#include <vector>
#include <thread>
#include <iostream>

#include "../include/MemoryPool.h"
#include "../include/HashBucket.h"

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

                for (size_t i = 0; i < ntimes; i++) {
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

    for (auto& t : vthread) t.join();
    printf("%lu个线程并发执行%lu轮次, 每轮次newElement&deleteElement %lu次, 总计花费：%lu ms\n", nworks, rounds, ntimes, total_cost_time);
}

void BenchmarkNew(size_t ntimes, size_t nworks, size_t rounds) {
	size_t total_cost_time = 0;
	std::vector<std::thread> vthread(nworks);

	for (size_t k = 0; k < nworks; k++) {
		vthread[k] = std::thread([&]() {
			for (size_t j = 0; j < rounds; j++) {
				size_t begin1 = clock();

				for (size_t i = 0; i < ntimes; i++) {
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

	for (auto& t : vthread) t.join();
	printf("%lu个线程并发执行%lu轮次, 每轮次malloc&free %lu次, 总计花费：%lu ms\n", nworks, rounds, ntimes, total_cost_time);
}

int main() {
    MemoryPool::HashBucket::initMemoryPool();
    BenchmarkMemoryPool(100, 1, 10);
	std::cout << "===========================================================================" << std::endl;
	std::cout << "===========================================================================" << std::endl;
    BenchmarkNew(100, 1, 10);
    return 0;
}