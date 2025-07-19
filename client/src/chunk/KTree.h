#pragma once

#include "KTreeNode.h"

template<uint32_t Layers, uint32_t NodeSize, typename TData>
class KTree {
public:
    const TData& get(uint32_t x, uint32_t y, uint32_t z) const { return m_root.get(x, y, z); }

    void set(uint32_t x, uint32_t y, uint32_t z, const TData& tData) { m_root.set(x, y, z, tData); }

private:
    KTreeNode<Layers, NodeSize, TData> m_root;
};