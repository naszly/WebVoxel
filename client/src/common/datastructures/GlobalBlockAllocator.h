#pragma once
#include "ThreadLocalBlockAllocator.h"

class GlobalBlockAllocator {
public:
    template<size_t BlockSize>
    static ThreadLocalBlockAllocator<BlockSize>& getAllocator();
};
