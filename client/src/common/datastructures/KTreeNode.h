#pragma once

#include <algorithm>
#include <cassert>
#include <iosfwd>
#include <istream>
#include <ostream>
#include <utility>
#include <cstring>

#include "Bitmap.h"
#include "common/Concetps.h"
#include "common/Utils.h"

#define USE_GLOBAL_BLOCK_ALLOCATOR

#ifdef USE_GLOBAL_BLOCK_ALLOCATOR
#include "GlobalBlockAllocator.h"
#endif

template<uint32_t Layer, uint32_t NodeCountPerAxis, typename TData>
    requires std::is_class_v<TData> && HasEmptyTrait<TData>
class KTreeNode {
    friend class KTreeNode<Layer + 1, NodeCountPerAxis, TData>;
    static constexpr TData EMPTY{};
    static constexpr bool IS_LEAF = Layer == 0;
    static constexpr uint32_t TREE_SIZE = Utils::pow(NodeCountPerAxis, Layer);
    static constexpr uint32_t NODE_COUNT = NodeCountPerAxis * NodeCountPerAxis * NodeCountPerAxis;
    using KTreeChildNode = KTreeNode<Layer - 1, NodeCountPerAxis, TData>;
    using NodeType = std::conditional_t<IS_LEAF, TData, KTreeChildNode*>;
    using NodeBitmap = Bitmap<NODE_COUNT, uint8_t>;
    static_assert(std::is_trivially_copyable_v<TData>, "KTree requires TData to be trivially copyable for safe serialization/deserialization.");
    static_assert(std::is_trivially_copyable_v<NodeType>, "KTree requires NodeType to be trivially copyable for fast moves.");
public:
    KTreeNode() noexcept {
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

    template<typename TOtherData>
    explicit KTreeNode(const KTreeNode<Layer, NodeCountPerAxis, TOtherData>& other) noexcept {
        copyFrom(other);
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

    ~KTreeNode() noexcept {
        clear();
    }

    [[nodiscard]] const TData& get(const uint32_t x, const uint32_t y, const uint32_t z) const noexcept {
        if constexpr (IS_LEAF) {
            assert(x < NodeCountPerAxis && y < NodeCountPerAxis && z < NodeCountPerAxis);
            [[assume(x < NodeCountPerAxis && y < NodeCountPerAxis && z < NodeCountPerAxis)]];
            return m_nodes[indexOf(x, y, z)];
        } else {
            const uint32_t nodeX = x / TREE_SIZE;
            const uint32_t nodeY = y / TREE_SIZE;
            const uint32_t nodeZ = z / TREE_SIZE;

            assert(nodeX < NodeCountPerAxis && nodeY < NodeCountPerAxis && nodeZ < NodeCountPerAxis);
            [[assume(nodeX < NodeCountPerAxis && nodeY < NodeCountPerAxis && nodeZ < NodeCountPerAxis)]];

            const auto* child = m_nodes[indexOf(nodeX, nodeY, nodeZ)];
            return child ? child->get(x % TREE_SIZE, y % TREE_SIZE, z % TREE_SIZE) : EMPTY;
        }
    }

    void set(const uint32_t x, const uint32_t y, const uint32_t z, const TData& tData) noexcept {
        if constexpr (IS_LEAF) {
            assert(x < NodeCountPerAxis && y < NodeCountPerAxis && z < NodeCountPerAxis);
            [[assume(x < NodeCountPerAxis && y < NodeCountPerAxis && z < NodeCountPerAxis)]];
            m_nodes[indexOf(x, y, z)] = tData;
        } else {
            const uint32_t nodeX = x / TREE_SIZE;
            const uint32_t nodeY = y / TREE_SIZE;
            const uint32_t nodeZ = z / TREE_SIZE;

            assert(nodeX < NodeCountPerAxis && nodeY < NodeCountPerAxis && nodeZ < NodeCountPerAxis);
            [[assume(nodeX < NodeCountPerAxis && nodeY < NodeCountPerAxis && nodeZ < NodeCountPerAxis)]];

            auto*& child = m_nodes[indexOf(nodeX, nodeY, nodeZ)];
            if (!child && !tData.isEmpty()) {
                child = allocateChildNode();
            }
            if (child) {
                child->set(x % TREE_SIZE, y % TREE_SIZE, z % TREE_SIZE, tData);
            }
        }
    }

    template<typename Func>
    void forEach(Func&& func) {
        if constexpr (IS_LEAF) {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                func(m_nodes[idx]);
            }
        } else {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (auto* child = m_nodes[idx]) {
                    child->forEach(func);
                }
            }
        }
    }

    template<typename Func>
    void forEach(Func&& func) const {
        if constexpr (IS_LEAF) {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                func(m_nodes[idx]);
            }
        } else {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (auto* child = m_nodes[idx]) {
                    child->forEach(func);
                }
            }
        }
    }

    void serialize(std::ostream& os) const {
        if constexpr (IS_LEAF) {
            os.write(reinterpret_cast<const char*>(m_nodes), NODE_COUNT * sizeof(TData));
        } else {
            NodeBitmap hasChildrenBitmap;

            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (m_nodes[idx] != nullptr)
                    hasChildrenBitmap.set(idx);
            }

            os.write(hasChildrenBitmap.data(), hasChildrenBitmap.size());

            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (const auto* child = m_nodes[idx])
                    child->serialize(os);
            }
        }
    }

    void deserialize(std::istream& is) {
        if constexpr (IS_LEAF) {
            is.read(reinterpret_cast<char*>(m_nodes), NODE_COUNT * sizeof(TData));
        } else {
            NodeBitmap hasChildrenBitmap;
            is.read(hasChildrenBitmap.data(), hasChildrenBitmap.size());
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (hasChildrenBitmap.test(idx)) {
                    auto*& child = m_nodes[idx];
                    if (!child) {
                        child = allocateChildNode();
                    }
                    child->deserialize(is);
                }
            }
        }
    }

    void removeEmptyNodes() noexcept {
        if constexpr (!IS_LEAF) {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (auto* child = m_nodes[idx]) {
                    if (child->isEmpty()) {
                        deallocateChildNode(child);
                        m_nodes[idx] = nullptr;
                    } else {
                        child->removeEmptyNodes();
                    }
                }
            }
        }
    }

    [[nodiscard]] bool isEmpty() const noexcept {
        if constexpr (IS_LEAF) {
            return std::all_of(m_nodes, m_nodes + NODE_COUNT, [](const TData& node) {
                return node.isEmpty();
            });
        } else {
            return std::all_of(m_nodes, m_nodes + NODE_COUNT, [](const KTreeChildNode* child) {
                return child == nullptr || child->isEmpty();
            });
        }
    }

    [[nodiscard]] const NodeType* getNodes() const noexcept {
        return m_nodes;
    }

private:
    NodeType m_nodes[NODE_COUNT]{};

    static constexpr uint32_t indexOf(const uint32_t x, const uint32_t y, const uint32_t z) noexcept {
        return (x * NodeCountPerAxis + y) * NodeCountPerAxis + z;
    }

    static KTreeChildNode* allocateChildNode() noexcept {
#if defined(USE_GLOBAL_BLOCK_ALLOCATOR)
        return new (GlobalBlockAllocator::getAllocator<sizeof(KTreeChildNode)>().allocate()) KTreeChildNode();
#else
        return new KTreeChildNode();
#endif
    }

    template<typename TSourceData>
    static KTreeChildNode* allocateChildNode(const KTreeNode<Layer-1, NodeCountPerAxis, TSourceData>& otherChild) noexcept {
#if defined(USE_GLOBAL_BLOCK_ALLOCATOR)
        return new (GlobalBlockAllocator::getAllocator<sizeof(KTreeChildNode)>().allocate()) KTreeChildNode(otherChild);
#else
        return new KTreeChildNode(otherChild);
#endif
    }

    static void deallocateChildNode(KTreeChildNode* child) noexcept {
#if defined(USE_GLOBAL_BLOCK_ALLOCATOR)
        child->~KTreeChildNode();
        GlobalBlockAllocator::getAllocator<sizeof(KTreeChildNode)>().deallocate(child);
#else
        delete child;
#endif
    }

    void initialize() noexcept {
        if constexpr (IS_LEAF) {
            std::fill_n(m_nodes, NODE_COUNT, EMPTY);
        } else {
            std::fill_n(m_nodes, NODE_COUNT, nullptr);
        }
    }

    void clear() noexcept {
        if constexpr (!IS_LEAF) {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (m_nodes[idx]) {
                    deallocateChildNode(m_nodes[idx]);
                    m_nodes[idx] = nullptr;
                }
            }
        }
    }

    template<typename TSourceData>
    void copyFrom(const KTreeNode<Layer, NodeCountPerAxis, TSourceData>& source) noexcept {
        if constexpr (IS_LEAF) {
            if constexpr (std::is_same_v<TData, TSourceData>) {
                std::memcpy(m_nodes, source.getNodes(), NODE_COUNT * sizeof(TData));
            } else {
                for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                    m_nodes[idx] = static_cast<TData>(source.getNodes()[idx]);
                }
            }
        } else {
            for (uint32_t idx = 0; idx < NODE_COUNT; ++idx) {
                if (const auto* otherChild = source.getNodes()[idx]) {
                    m_nodes[idx] = allocateChildNode(*otherChild);
                } else {
                    m_nodes[idx] = nullptr;
                }
            }
        }
    }

    void moveFrom(KTreeNode&& other) noexcept {
        std::memcpy(m_nodes, other.m_nodes, NODE_COUNT * sizeof(NodeType));
        if constexpr (IS_LEAF) {
            std::fill_n(other.m_nodes, NODE_COUNT, EMPTY);
        } else {
            std::fill_n(other.m_nodes, NODE_COUNT, nullptr);
        }
    }
};
