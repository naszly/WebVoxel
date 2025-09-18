#pragma once

#include <cstddef>
#include "common/Thread.h"
#include "common/datastructures/AtomicQueue.h"
#include "common/datastructures/CircularBuffer.h"

template<size_t BlockSize, size_t BlocksPerChunk = 4096>
class ThreadLocalBlockAllocator {
public:
    ThreadLocalBlockAllocator() = default;
    ~ThreadLocalBlockAllocator() {
        while (auto chunkPtr = m_chunks.tryPop()) {
            free(chunkPtr.value());
        }
    }

    void* allocate() {
        if (const auto ptr = m_threadLocalFreeList.tryPop()) {
            return ptr.value();
        }

        if (const auto ptr = m_globalFreeList.tryPop()) {
            return ptr.value();
        }

        return allocateNewChunk();
    }

    void deallocate(void* ptr) {
        if (m_threadLocalFreeList.full()) {
            spillToGlobalFreeList();
        }

        m_threadLocalFreeList.push(ptr);
    }

private:
    static constexpr size_t MAX_THREAD_LOCAL_BLOCKS = BlocksPerChunk * 32;

    AtomicQueue<std::byte*> m_chunks;
    AtomicQueue<void*> m_globalFreeList;

    static thread_local CircularBuffer<void*, MAX_THREAD_LOCAL_BLOCKS> m_threadLocalFreeList;

    void* allocateNewChunk() {
        constexpr std::size_t alignment = alignof(std::max_align_t);
        constexpr std::size_t chunkSize = BlockSize * BlocksPerChunk;
        const auto newChunk = static_cast<std::byte*>(aligned_alloc(alignment, chunkSize));

        m_chunks.push(newChunk);
        populateThreadLocalFreeList(newChunk);

        return newChunk;
    }

    static void populateThreadLocalFreeList(std::byte* newChunk) {
        const std::byte* end = newChunk + BlockSize * BlocksPerChunk;
        for (std::byte* ptr = newChunk + BlockSize; ptr < end; ptr += BlockSize) {
            m_threadLocalFreeList.push(ptr);
        }
    }

    void spillToGlobalFreeList() {
        void* freeList[BlocksPerChunk];
        for (size_t i = 0; i < BlocksPerChunk; ++i) {
            freeList[i] = m_threadLocalFreeList.pop();
        }
        m_globalFreeList.pushMultiple(freeList);
    }
};

template<size_t BlockSize, size_t BlocksPerChunk>
thread_local CircularBuffer<void*, ThreadLocalBlockAllocator<BlockSize, BlocksPerChunk>::MAX_THREAD_LOCAL_BLOCKS>
    ThreadLocalBlockAllocator<BlockSize, BlocksPerChunk>::m_threadLocalFreeList = {};
