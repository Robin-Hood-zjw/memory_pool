# High-concurrency Memory Pool
A memory pool is a memory allocation optimization technique. It pre-allocates a large block of memory from the system; subsequently, during program execution, it directly and rapidly allocates smaller memory blocks from this pool, thereby avoiding frequent calls to `malloc`/`free` or `new`/`delete`.

---

## Features

### Organization
- **ThreadCache**: ThreadCache is per-thread local memory that reduces inter-thread contention and improves the efficiency of small object allocation.
- **CentralCache**: CentralCache is responsible for global caching and for the redistribution of memory blocks among various threads.
- **PageCache**: PageCache allocates large blocks of memory from the system and provides them for use by higher-level caches.

### Objectives
* avoid frequent system calls
* reduce external memory fragmentation
* enhance performance through faster allocations
* ensure deterministic, stable behavior in high-performance applications

---

## Functionalities

### 

- **ThreadCache**：Once each thread has retrieved its own Thread-Local Storage (TLS) variable, whenever that thread requests memory, the ThreadCache object fetches a specified number of memory objects from the corresponding free list and returns them to the user. No locking is required here; by utilizing Thread-Local Storage, each thread possesses its own unique ThreadCache object dedicated to maintaining its free list.
- **CentralCache**：The CentralCache is shared by all threads, while the ThreadCache acquires memory objects from the CentralCache on an as-needed basis. The CentralCache conditionally reclaims memory objects from the ThreadCache to prevent a single thread from monopolizing an excessive number of objects—a scenario that would leave other threads with insufficient resources and lead to efficiency issues. This level of caching serves to achieve a more balanced, demand-driven allocation of memory across multiple threads. Since the CentralCache is a shared resource subject to contention, accessing it to retrieve memory objects requires the use of locks.
- **PageCache**: This serves as an additional caching layer built atop the CentralCache; memory within it is stored and allocated in units of pages. When the CentralCache lacks available Span objects, the PageCache allocates a specific number of pages, subdivides them into fixed-size blocks of memory, and assigns them to the CentralCache. Furthermore, the PageCache reclaims eligible Span objects from the CentralCache and merges adjacent pages to form larger pages, thereby mitigating the issue of memory fragmentation. 

### 四.模快接口

* 线程缓存：包括一个自由链表数组。ThreadCache提供的接口主要有:
  * Allocate：从ThreadCache分配内存
  * Deallocate：释放内存给ThreadCache
  * FetchFromCentralCache：向中心缓存区获取内存对象
  * ListTooLong：当自由链表中的对象个数超过一定数量，然后将内存对象归还给CentralCache,为了实现均衡技术，防止其他线程要获取该大小的内存对象还需要去PageCache中去获取，从而带来效率问题
* 中心缓存：包括一个Span对象的双向带头循环链表数组，每一个Span对象由多页(一页4k)组成，Span对象中也包含了一条自由链表。CentralCache提供的主要接口有：
  * GetInstance：为了保证全局只有唯一的一个CentralCache,本模快采用单例模式(饿汉模式)实现，此接口用来获取单例对象
  * FetchRangeObj：获取一定数量的内存对象给ThreadCache
  * GetOneSpan：从对应位置上获取一个Span对象，如果对象位置没有Span对象，则去向PageCache去申请Span对象，该接口主要是给FetchRangeObj使用
  * ReleaseListToSpans：将ThreadCache中过长的自由链表中的内存对象重新挂到对应的Span中，主要在ThreadCache中的ListTooLong中被调用，为了均衡个县城之间相同大小的内存对象数量
* 页缓存：包含一个由Span对象构成的SpanList数组，大小为128，如果申请的内存超过128页，则直接去向系统去申请。PageCache提供的接口主要有：
  * GetInstance：为了保证全局只有唯一的一个PageCache对象，本模快使用单例模式(饿汉模式)实现，此接口用来获取单例对象
  * NewSpan：如果用户申请超过128页的内存，则直接去向系统申请，如果申请的内存是64k-128页的内存，ThreadCache直接调用该接口获取到内存对象，如果申请的内存小于64k，首先去ThreadCache申请，如果没有则逐级向上申请，该接口主要用在CentralCache中的GetOneSpan中和申请内存范围在64k-128页中
  * MapObjectToSpan：当内存对象从ThreadCache归还给CentralCache时，每一个内存对象都需要知道自由是属于哪一个Span对象的，所以在PageCache合并Span或者是从系统申请出来的Span，都要建立一个页号到Span的映射，在归还的时候，可以通过查找这个映射关系来确定是在哪一个Span中，本接口就是为了查找映射关系
  * RelaseToPageCache：每一个Span对象中都存在着一个使用计数，当这个使用计数为0时说明该Span中的所有内存对象都是空闲的。此时，可以将这个Span对象归还给PageCache中进行合并来减少内存碎片。本接口主要在CentralCache中的ReleaseListToSpans被调用，用来归还Span对象和合并相邻空闲的Span来减少内存碎片

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