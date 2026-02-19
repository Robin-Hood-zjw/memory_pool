#pragma once

#include <atomic>

struct Slot {
    std::atomic<Slot*> next;
};

