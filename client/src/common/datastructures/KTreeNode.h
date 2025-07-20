#pragma once

#include <algorithm>
#include <cassert>
#include <iosfwd>
#include <istream>
#include <ostream>
#include <utility>

#include "Bitmap.h"
#include "common/Utils.h"

template<typename T>
concept HasIsEmpty = requires(T t) {
    { t.isEmpty() } -> std::convertible_to<bool>;
};

template<uint32_t Layer, uint32_t NodeCountPerAxis, typename TData>
    requires std::is_class_v<TData> && HasIsEmpty<TData>
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
            NodeBitmap hasChildrenBitmap;

            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if ((&m_nodes[0][0][0])[idx] != nullptr)
                    hasChildrenBitmap.set(idx);
            }

            os.write(hasChildrenBitmap.data(), hasChildrenBitmap.size());

            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (const auto* child = (&m_nodes[0][0][0])[idx])
                    child->serialize(os);
            }
        }
    }

    void deserialize(std::istream& is) {
        if constexpr (IS_LEAF) {
            is.read(reinterpret_cast<char*>(&m_nodes[0][0][0]), NODE_COUNT * sizeof(TData));
        } else {
            NodeBitmap hasChildrenBitmap;
            is.read(hasChildrenBitmap.data(), hasChildrenBitmap.size());

            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (hasChildrenBitmap.test(idx)) {
                    auto*& child = (&m_nodes[0][0][0])[idx];
                    if (!child) {
                        child = new KTreeChildNode();
                    }
                    child->deserialize(is);
                }
            }
        }
    }

    [[nodiscard]] size_t getSerializedSize() const {
        if constexpr (IS_LEAF) {
            return NODE_COUNT * sizeof(TData);
        } else {
            size_t size = NodeBitmap::size();

            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (const auto* child = (&m_nodes[0][0][0])[idx])
                    size += child->getSerializedSize();
            }

            return size;
        }
    }

private:
    static constexpr TData EMPTY{};
    static constexpr bool IS_LEAF = Layer == 0;
    static constexpr uint32_t TREE_SIZE = Utils::pow(NodeCountPerAxis, Layer);
    static constexpr uint32_t NODE_COUNT = NodeCountPerAxis * NodeCountPerAxis * NodeCountPerAxis;
    using KTreeChildNode = KTreeNode<Layer - 1, NodeCountPerAxis, TData>;
    using NodeType = std::conditional_t<IS_LEAF, TData, KTreeChildNode*>;
    using NodeBitmap = Bitmap<NODE_COUNT, uint8_t>;

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
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                delete std::exchange((&m_nodes[0][0][0])[idx], nullptr);
            }
        }
    }

    void copyFrom(const KTreeNode& other) {
        if constexpr (IS_LEAF) {
            std::copy_n(&other.m_nodes[0][0][0], NODE_COUNT, &m_nodes[0][0][0]);
        } else {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (const auto* otherChild = (&other.m_nodes[0][0][0])[idx]) {
                    (&m_nodes[0][0][0])[idx] = new KTreeChildNode(*otherChild);
                } else {
                    (&m_nodes[0][0][0])[idx] = nullptr;
                }
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
};
