#pragma once

#include <algorithm>
#include <cassert>
#include <iosfwd>
#include <istream>
#include <ostream>
#include <utility>

#include "../Utils.h"


template<uint32_t Layer, uint32_t NodeCountPerAxis, typename TData, std::enable_if_t<std::is_class_v<TData>, int> = 0>
class KTreeNode {
    friend class KTreeNode<Layer + 1, NodeCountPerAxis, TData>;
public:
    KTreeNode() {
        initialize();
    }

    KTreeNode(const KTreeNode& other) noexcept {
        copyFrom(other);
    }

    KTreeNode& operator=(const KTreeNode& other) noexcept {
        if (this != &other) {
            clear();
            copyFrom(other);
        }
        return *this;
    }

    KTreeNode(KTreeNode&& other) noexcept {
        moveFrom(std::move(other));
    }

    KTreeNode& operator=(KTreeNode&& other) noexcept {
        if (this != &other) {
            clear();
            moveFrom(std::move(other));
        }
        return *this;
    }

    ~KTreeNode() {
        clear();
    }

    [[nodiscard]] const TData& get(uint32_t x, uint32_t y, uint32_t z) const {
        if constexpr (IS_LEAF) {
            assert(isInBounds(x, y, z));
            [[assume(isInBounds(x, y, z))]];
            return m_nodes[x][y][z];
        } else {
            const uint32_t nodeX = x / TREE_SIZE;
            const uint32_t nodeY = y / TREE_SIZE;
            const uint32_t nodeZ = z / TREE_SIZE;

            assert(isInBounds(nodeX, nodeY, nodeZ));
            [[assume(isInBounds(nodeX, nodeY, nodeZ))]];

            const auto* child = m_nodes[nodeX][nodeY][nodeZ];
            return child ? child->get(x % TREE_SIZE, y % TREE_SIZE, z % TREE_SIZE) : EMPTY;
        }
    }

    void set(uint32_t x, uint32_t y, uint32_t z, const TData& tData) {
        if constexpr (IS_LEAF) {
            assert(isInBounds(x, y, z));
            [[assume(isInBounds(x, y, z))]];
            m_nodes[x][y][z] = tData;
        } else {
            const uint32_t nodeX = x / TREE_SIZE;
            const uint32_t nodeY = y / TREE_SIZE;
            const uint32_t nodeZ = z / TREE_SIZE;

            assert(isInBounds(nodeX, nodeY, nodeZ));
            [[assume(isInBounds(nodeX, nodeY, nodeZ))]];

            auto*& child = m_nodes[nodeX][nodeY][nodeZ];
            if (!child && !tData.isEmpty()) {
                child = new KTreeChildNode();
            }

            if (child) {
                child->set(x % TREE_SIZE, y % TREE_SIZE, z % TREE_SIZE, tData);
            }
        }
    }

    void serialize(std::ostream& os) const {
        if constexpr (IS_LEAF) {
            os.write(reinterpret_cast<const char*>(&m_nodes[0][0][0]), NODE_COUNT * sizeof(TData));
        } else {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                const auto* child = (&m_nodes[0][0][0])[idx];
                bool hasChild = (child != nullptr);
                os.write(reinterpret_cast<const char*>(&hasChild), sizeof(bool));
                if (hasChild) {
                    child->serialize(os);
                }
            }
        }
    }

    void deserialize(std::istream& is) {
        if constexpr (IS_LEAF) {
            is.read(reinterpret_cast<char*>(&m_nodes[0][0][0]), NODE_COUNT * sizeof(TData));
        } else {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                bool hasChild = false;
                is.read(reinterpret_cast<char*>(&hasChild), sizeof(bool));

                if (hasChild) {
                    auto*& child = (&m_nodes[0][0][0])[idx];
                    if (!child) {
                        child = new KTreeChildNode();
                    }
                    child->deserialize(is);
                }
            }
        }
    }

private:
    static constexpr TData EMPTY{};
    static constexpr bool IS_LEAF = Layer == 0;
    static constexpr uint32_t TREE_SIZE = Utils::pow(NodeCountPerAxis, Layer);
    using KTreeChildNode = KTreeNode<Layer - 1, NodeCountPerAxis, TData>;
    using NodeType = std::conditional_t<IS_LEAF, TData, KTreeChildNode*>;

    NodeType m_nodes[NodeCountPerAxis][NodeCountPerAxis][NodeCountPerAxis]{};

    [[nodiscard]] static constexpr bool isInBounds(const uint32_t x, const uint32_t y, const uint32_t z) {
        return x < NodeCountPerAxis && y < NodeCountPerAxis && z < NodeCountPerAxis;
    }

    void initialize() {
        if constexpr (IS_LEAF) {
            std::fill_n(&m_nodes[0][0][0], NODE_COUNT, EMPTY);
        } else {
            std::fill_n(&m_nodes[0][0][0], NODE_COUNT, nullptr);
        }
    }

    void clear() {
        if constexpr (!IS_LEAF) {
            for (uint32_t x = 0; x < NodeCountPerAxis; ++x)
                for (uint32_t y = 0; y < NodeCountPerAxis; ++y)
                    for (uint32_t z = 0; z < NodeCountPerAxis; ++z)
                        delete std::exchange(m_nodes[x][y][z], nullptr);
        }
    }

    void copyFrom(const KTreeNode& other) {
        if constexpr (IS_LEAF) {
            std::copy_n(&other.m_nodes[0][0][0], NODE_COUNT, &m_nodes[0][0][0]);
        } else {
            std::fill_n(&m_nodes[0][0][0], NODE_COUNT, nullptr);
            for (uint32_t x = 0; x < NodeCountPerAxis; ++x)
                for (uint32_t y = 0; y < NodeCountPerAxis; ++y)
                    for (uint32_t z = 0; z < NodeCountPerAxis; ++z)
                        if (const auto* otherChild = other.m_nodes[x][y][z]) {
                            m_nodes[x][y][z] = new KTreeChildNode(*otherChild);
                        }
        }
    }

    void moveFrom(KTreeNode&& other) {
        std::copy_n(&other.m_nodes[0][0][0], NODE_COUNT, &m_nodes[0][0][0]);
        if constexpr (IS_LEAF) {
            std::fill_n(&other.m_nodes[0][0][0], NODE_COUNT, EMPTY);
        } else {
            std::fill_n(&other.m_nodes[0][0][0], NODE_COUNT, nullptr);
        }
    }

    static constexpr uint32_t NODE_COUNT = NodeCountPerAxis * NodeCountPerAxis * NodeCountPerAxis;
};
