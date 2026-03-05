#include "../include/Common.h"

namespace Pool {
    size_t SizeClass::roundUp(size_t bytes) {
        return (bytes + ALIGNMENT - 1) / ALIGNMENT * ALIGNMENT;
    };

    size_t SizeClass::getIndex(size_t bytes) {
        bytes = std::max(bytes, ALIGNMENT);
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    };
}
