#include "GlobalBlockAllocator.h"

#include "IdMappedKTree.h"
#include "ThreadLocalBlockAllocator.h"

static ThreadLocalBlockAllocator<8> allocator8;
static ThreadLocalBlockAllocator<16> allocator16;
static ThreadLocalBlockAllocator<32> allocator32;
static ThreadLocalBlockAllocator<64> allocator64;
static ThreadLocalBlockAllocator<sizeof(IdMappedKTree<>)> allocatorKTree;

template<>
ThreadLocalBlockAllocator<8>& GlobalBlockAllocator::getAllocator<8>() {
    return allocator8;
}

template<>
ThreadLocalBlockAllocator<16>& GlobalBlockAllocator::getAllocator<16>() {
    return allocator16;
}

template<>
ThreadLocalBlockAllocator<32>& GlobalBlockAllocator::getAllocator<32>() {
    return allocator32;
}

template<>
ThreadLocalBlockAllocator<64>& GlobalBlockAllocator::getAllocator<64>() {
    return allocator64;
}

template<>
ThreadLocalBlockAllocator<sizeof(IdMappedKTree<>)>& GlobalBlockAllocator::getAllocator<sizeof(IdMappedKTree<>)>() {
    return allocatorKTree;
}
