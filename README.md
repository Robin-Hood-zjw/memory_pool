# High-concurrency Memory Pool
A memory pool is a memory allocation optimization technique. It pre-allocates a large block of memory from the system; subsequently, during program execution, it directly and rapidly allocates smaller memory blocks from this pool, thereby avoiding frequent calls to `malloc`/`free` or `new`/`delete`.

---

## Features

### Organization
- **PageCache**: PageCache allocates large blocks of memory from the system and provides them for use by higher-level caches.
- **CentralCache**: CentralCache is responsible for global caching and for the redistribution of memory blocks among various threads.
- **ThreadCache**: ThreadCache is per-thread local memory that reduces inter-thread contention and improves the efficiency of small object allocation.

### Objectives
* avoid frequent system calls
* reduce external memory fragmentation
* enhance performance through faster allocations
* ensure deterministic, stable behavior in high-performance applications

### Functionality
- **ThreadCache**：
    * no lock is required since atomic pointers offer faster execution
    * TLS (thread-local storage) and singleton pattern protects the safety in each thread
    * fetch a specified number of memory objects from the free list and returns them to the user
- **CentralCache**：
    * coordinate the needs between ThreadCache and PageCache
        * give small memory blocks to ThreadCache and recycle the blocks from them
        * request large memory blocks from PageCache and cut them, and recycle them to return to PageCache
    * spin lock with atomic_flag shrink the contention in one SizeClass free list
- **PageCache**: 
    * schedule global memory through a global mutex lock
    * request memory and organize in the format of Span, then send them to CentralCache
    * recycle Spans from CentralCache when some conditions are triggered

---

## Environment

- **Operating System:** Ubuntu 22.04 LTS  
- **Build System:** CMake  
- **Compiler:** g++ / clang++

---

## Build & Run

```bash
# create build directory
mkdir build && cd build

# generate build files
cmake ..

# build project
make

# run executable
./main