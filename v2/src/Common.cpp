#include "../include/Common.h"

namespace Pool {
    // increment the requested bytes up to a multiple of the nearest ALIGNMENT
    size_t SizeClass::roundUp(size_t bytes) {
        return (bytes + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
    };

    // calculate the index of the target MemoryPool in the HashBucket
    size_t SizeClass::getIndex(size_t bytes) {
        bytes = std::max(bytes, ALIGNMENT);
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    };
}
