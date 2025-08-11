#pragma once

#include <cstddef>
#include <vector>
#include "common/Thread.h"
#include "common/datastructures/AtomicQueue.h"

template<size_t BlockSize, size_t BlocksPerChunk = 4096>
class ThreadLocalBlockAllocator {
public:
    ThreadLocalBlockAllocator() = default;
    ~ThreadLocalBlockAllocator() {
        std::byte* chunkPtr;
        while ((chunkPtr = m_chunks.pop()) != nullptr) {
            std::free(chunkPtr);
        }
    }

    void* allocate() {
        if (void* ptr = tryAllocateFromThreadFreeList()) {
            return ptr;
        }

        if (void* ptr = tryAllocateFromGlobalFreeList()) {
            return ptr;
        }

        return allocateNewChunk();
    }

    void deallocate(void* ptr) {
        m_threadLocalFreeList.push_back(ptr);
        if (m_threadLocalFreeList.size() > BlocksPerChunk && m_globalFreeList.empty()) {
            const size_t half = m_threadLocalFreeList.size() / 2;
            for (size_t i = 0; i < half; ++i) {
                m_globalFreeList.push(m_threadLocalFreeList[i]);
            }
            m_threadLocalFreeList.erase(m_threadLocalFreeList.begin(), m_threadLocalFreeList.begin() + half);
        }
    }

private:
    AtomicQueue<std::byte*> m_chunks;
    AtomicQueue<void*> m_globalFreeList;
    static thread_local std::vector<void*> m_threadLocalFreeList;

    static void* tryAllocateFromThreadFreeList() {
        if (!m_threadLocalFreeList.empty()) {
            void* ptr = m_threadLocalFreeList.back();
            m_threadLocalFreeList.pop_back();
            return ptr;
        }
        return nullptr;
    }

    void* tryAllocateFromGlobalFreeList() {
        return m_globalFreeList.pop();
    }

    void* allocateNewChunk() {
        constexpr std::size_t alignment = alignof(std::max_align_t);
        constexpr std::size_t chunkSize = BlockSize * BlocksPerChunk;
        const auto newChunk = static_cast<std::byte*>(std::aligned_alloc(alignment, chunkSize));

        addChunk(newChunk);
        populateThreadLocalFreeList(newChunk);

        return newChunk;
    }

    void addChunk(std::byte* chunk) {
        m_chunks.push(chunk);
    }

    static void populateThreadLocalFreeList(std::byte* newChunk) {
        m_threadLocalFreeList.reserve(m_threadLocalFreeList.size() + BlocksPerChunk - 1);
        const std::byte* end = newChunk + BlockSize * BlocksPerChunk;
        for (std::byte* ptr = newChunk + BlockSize; ptr < end; ptr += BlockSize) {
            m_threadLocalFreeList.push_back(ptr);
        }
    }
};

template<size_t BlockSize, size_t BlocksPerChunk>
thread_local std::vector<void*> ThreadLocalBlockAllocator<BlockSize, BlocksPerChunk>::m_threadLocalFreeList;
