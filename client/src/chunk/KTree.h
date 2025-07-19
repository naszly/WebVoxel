#pragma once

#include "KTreeNode.h"

template<uint32_t Layers, uint32_t NodeSize, typename TData>
class KTree {
public:
    const TData& get(uint32_t x, uint32_t y, uint32_t z) const { return m_root.get(x, y, z); }

    void set(uint32_t x, uint32_t y, uint32_t z, const TData& tData) { m_root.set(x, y, z, tData); }

    void serialize(std::ostringstream& os) const { m_root.serialize(os); }

    void deserialize(std::istringstream& is) { m_root.deserialize(is); }

private:
    KTreeNode<Layers, NodeSize, TData> m_root;
};