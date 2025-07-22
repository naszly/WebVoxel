#pragma once

#include "KTreeNode.h"

template<uint32_t Layers, uint32_t NodeCountPerAxis, typename TData>
    requires std::is_class_v<TData> && HasIsEmpty<TData>
class KTree {
public:
    [[nodiscard]] const TData& get(uint32_t x, uint32_t y, uint32_t z) const { return m_root.get(x, y, z); }

    void set(uint32_t x, uint32_t y, uint32_t z, const TData& tData) { m_root.set(x, y, z, tData); }

    void serialize(std::ostringstream& os) {
        m_root.removeEmptyNodes();
        m_root.serialize(os);
    }

    void deserialize(std::istringstream& is) { m_root.deserialize(is); }

    static size_t getMaxSerializedSize() {
        return KTreeNode<Layers, NodeCountPerAxis, TData>::getMaxSerializedSize();
    }

    [[nodiscard]] bool isEmpty() const {
        return m_root.isEmpty();
    }

private:
    KTreeNode<Layers, NodeCountPerAxis, TData> m_root;
};