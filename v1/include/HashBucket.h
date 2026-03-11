#include "./MemoryPool.h"

namespace Pool {
    #define SLOT_BASE_SIZE 8                                                // the min size of each slot
    #define MAX_SLOT_SIZE 512                                               // the max size of each slot
    #define MEMORY_POOL_NUM 64                                              // the total number of all the memory pools

    class HashBucket {
        public:
            static void initMemoryPool();                                   // initialize all the memory pools
            static MemoryPool& getMemoryPool(int index);

            static void* useMemory(size_t size);                            // search the target memory pool and allocate a slot from the pool
            static void freeMemory(void* ptr, size_t size);                 // search the target memory pool and deallocate the slot into the free list

            template<typename T, typename... Args>
            friend T* newElement(Args&&... args);                           // factory method pattern: simulate to create a new object

            template<typename T>
            friend void deleteElement(T* p);                                // factory method pattern: simulate to delete an object
    };

    template<typename T, typename... Args>
    T* newElement(Args&&... args) {
        // allocate a memory block based on the size of object T
        T* p = nullptr;
        p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)));

        // call the constructor of T at the existing address pointed to by pointer "p"
        if (p) new(p) T(std::forward<Args>(args)...);
        return p;
    }

    template<typename T>
    void deleteElement(T* p) {
        if (p) {
            // call the object's destrcutor
            p->~T();
            // return this memory block to HashBucket
            HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
        }
    }
}