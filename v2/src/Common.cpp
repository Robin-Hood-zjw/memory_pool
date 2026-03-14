#include "../include/Common.h"

namespace Pool {
    /**
     * @brief standardizes a memory request p to a multiple of the nearest ALIGNMENT
     * @param bytes the raw number of bytes requested by the user
     * @return size_t the aligned size
     **/
    size_t SizeClass::roundUp(size_t bytes) {
        return (bytes + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
    };

    /**
     * @brief maps a byte size to a specific array index
     * @param bytes the number of bytes to be allocated
     * @return size_t the 0-based index of the corresponding MemoryPool
     **/

    // calculate the index of the target MemoryPool in the HashBucket
    size_t SizeClass::getIndex(size_t bytes) {
        bytes = std::max(bytes, ALIGNMENT);
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    };
}
