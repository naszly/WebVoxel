#pragma once

#include "KTreeNode.h"

template<uint32_t Layers, uint32_t NodeCountPerAxis, typename TData>
    requires std::is_class_v<TData> && HasEmptyTrait<TData>
class KTree {
    static_assert(std::is_trivially_copyable_v<TData>, "KTree requires TData to be trivially copyable for safe serialization/deserialization.");
public:
    KTree() = default;

    template<typename TOtherData>
    explicit KTree(const KTree<Layers, NodeCountPerAxis, TOtherData>& other) : m_root(other.getRoot()) {}

    [[nodiscard]] const TData& get(uint32_t x, uint32_t y, uint32_t z) const { return m_root.get(x, y, z); }

    void set(uint32_t x, uint32_t y, uint32_t z, const TData& tData) { m_root.set(x, y, z, tData); }

    template<typename Func>
    void forEach(Func&& func) {
        m_root.forEach(std::forward<Func>(func));
    }

    template<typename Func>
    void forEach(Func&& func) const {
        m_root.forEach(std::forward<Func>(func));
    }

    void serialize(std::ostream& os) {
        m_root.removeEmptyNodes();
        m_root.serialize(os);
    }

    void deserialize(std::istream& is) { m_root.deserialize(is); }

    [[nodiscard]] bool isEmpty() const {
        return m_root.isEmpty();
    }

    const KTreeNode<Layers, NodeCountPerAxis, TData>& getRoot() const {
        return m_root;
    }

private:
    KTreeNode<Layers, NodeCountPerAxis, TData> m_root;
};