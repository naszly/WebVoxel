#pragma once

#include "IdMappedKTree.h"
#include <cassert>
#include <ostream>
#include <istream>

template<typename DataType, uint32_t Depth, uint32_t BaseSize>
class DynamicallyMappedKTree {
    using Tree8 = IdMappedKTree<DataType, Depth, BaseSize, IdSize::U8Bit>;
    using Tree16 = IdMappedKTree<DataType, Depth, BaseSize, IdSize::U16Bit>;
    using Tree20 = IdMappedKTree<DataType, Depth, BaseSize, IdSize::U20Bit>;

    union TreeVariants {
        Tree8* u8Tree{nullptr};
        Tree16* u16Tree;
        Tree20* u20Tree;
    };

public:
    DynamicallyMappedKTree() {
        switch (m_idSize) {
            case IdSize::U8Bit: m_tree.u8Tree = new Tree8(); break;
            case IdSize::U16Bit: m_tree.u16Tree = new Tree16(); break;
            case IdSize::U20Bit: m_tree.u20Tree = new Tree20(); break;
        }
    }

    ~DynamicallyMappedKTree() {
        switch (m_idSize) {
            case IdSize::U8Bit: delete m_tree.u8Tree; break;
            case IdSize::U16Bit: delete m_tree.u16Tree; break;
            case IdSize::U20Bit: delete m_tree.u20Tree; break;
        }
    }

    DynamicallyMappedKTree(const DynamicallyMappedKTree& other) {
        m_idSize = other.m_idSize;
        switch (m_idSize) {
            case IdSize::U8Bit: m_tree.u8Tree = new Tree8(*other.m_tree.u8Tree); break;
            case IdSize::U16Bit: m_tree.u16Tree = new Tree16(*other.m_tree.u16Tree); break;
            case IdSize::U20Bit: m_tree.u20Tree = new Tree20(*other.m_tree.u20Tree); break;
        }
    }

    DynamicallyMappedKTree& operator=(const DynamicallyMappedKTree& other) {
        if (this != &other) {
            switch (m_idSize) {
                case IdSize::U8Bit: delete m_tree.u8Tree; break;
                case IdSize::U16Bit: delete m_tree.u16Tree; break;
                case IdSize::U20Bit: delete m_tree.u20Tree; break;
            }
            clearPointers();
            m_idSize = other.m_idSize;
            switch (m_idSize) {
                case IdSize::U8Bit: m_tree.u8Tree = new Tree8(*other.m_tree.u8Tree); break;
                case IdSize::U16Bit: m_tree.u16Tree = new Tree16(*other.m_tree.u16Tree); break;
                case IdSize::U20Bit: m_tree.u20Tree = new Tree20(*other.m_tree.u20Tree); break;
            }
        }
        return *this;
    }

    DynamicallyMappedKTree(DynamicallyMappedKTree&& other) noexcept {
        m_idSize = other.m_idSize;
        m_tree = other.m_tree;
        other.clearPointers();
    }

    DynamicallyMappedKTree& operator=(DynamicallyMappedKTree&& other) noexcept {
        if (this != &other) {
            switch (m_idSize) {
                case IdSize::U8Bit: delete m_tree.u8Tree; break;
                case IdSize::U16Bit: delete m_tree.u16Tree; break;
                case IdSize::U20Bit: delete m_tree.u20Tree; break;
            }
            m_idSize = other.m_idSize;
            m_tree = other.m_tree;
            other.clearPointers();
        }
        return *this;
    }

    const DataType& getData(uint32_t x, uint32_t y, uint32_t z) const {
        switch (m_idSize) {
            case IdSize::U8Bit: return m_tree.u8Tree->getData(x, y, z);
            case IdSize::U16Bit: return m_tree.u16Tree->getData(x, y, z);
            case IdSize::U20Bit: return m_tree.u20Tree->getData(x, y, z);
        }
        std::unreachable();
    }

    void setData(uint32_t x, uint32_t y, uint32_t z, const DataType& data) {
        switch (m_idSize) {
            case IdSize::U8Bit: {
                bool success = m_tree.u8Tree->trySetData(x, y, z, data);
                if (!success) {
                    updateIdSize(IdSize::U16Bit);
                    m_tree.u16Tree->trySetData(x, y, z, data);
                }
                break;
            }
            case IdSize::U16Bit: {
                bool success = m_tree.u16Tree->trySetData(x, y, z, data);
                if (!success) {
                    updateIdSize(IdSize::U20Bit);
                    m_tree.u20Tree->trySetData(x, y, z, data);
                }
                break;
            }
            case IdSize::U20Bit: {
                bool success = m_tree.u20Tree->trySetData(x, y, z, data);
                assert(success && "Failed to set data with U20Bit ID size");
                break;
            }
        }
    }

    [[nodiscard]] bool isEmpty() const {
        switch (m_idSize) {
            case IdSize::U8Bit: return m_tree.u8Tree->isEmpty();
            case IdSize::U16Bit: return m_tree.u16Tree->isEmpty();
            case IdSize::U20Bit: return m_tree.u20Tree->isEmpty();
        }
        std::unreachable();
    }

    void serialize(std::ostream& os) {
        optimizeDataToIdMapping();
        shrinkToMinimalIdSize();
        os.write(reinterpret_cast<const char*>(&m_idSize), sizeof(m_idSize));
        switch (m_idSize) {
            case IdSize::U8Bit: m_tree.u8Tree->serialize(os); break;
            case IdSize::U16Bit: m_tree.u16Tree->serialize(os); break;
            case IdSize::U20Bit: m_tree.u20Tree->serialize(os); break;
        }
    }

    void deserialize(std::istream& is) {
        IdSize idSize;
        is.read(reinterpret_cast<char*>(&idSize), sizeof(idSize));
        updateIdSize(idSize);
        switch (idSize) {
            case IdSize::U8Bit: m_tree.u8Tree->deserialize(is); break;
            case IdSize::U16Bit: m_tree.u16Tree->deserialize(is); break;
            case IdSize::U20Bit: m_tree.u20Tree->deserialize(is); break;
        }
    }

    void optimizeDataToIdMapping() {
        switch (m_idSize) {
            case IdSize::U8Bit: m_tree.u8Tree->optimizeDataToIdMapping(); break;
            case IdSize::U16Bit: m_tree.u16Tree->optimizeDataToIdMapping(); break;
            case IdSize::U20Bit: m_tree.u20Tree->optimizeDataToIdMapping(); break;
        }
    }

    void shrinkToMinimalIdSize() {
        IdSize optimalIdSize = m_idSize;
        switch (m_idSize) {
            case IdSize::U8Bit: optimalIdSize = m_tree.u8Tree->getMinIdSize(); break;
            case IdSize::U16Bit: optimalIdSize = m_tree.u16Tree->getMinIdSize(); break;
            case IdSize::U20Bit: optimalIdSize = m_tree.u20Tree->getMinIdSize(); break;
        }
        updateIdSize(optimalIdSize);
    }

private:
    IdSize m_idSize{IdSize::U8Bit};
    TreeVariants m_tree;

    void updateIdSize(const IdSize newIdSize) {
        if (m_idSize == newIdSize) return;
        switch (m_idSize) {
            case IdSize::U8Bit:
                if (newIdSize == IdSize::U16Bit) {
                    auto* newTree = new Tree16(*m_tree.u8Tree);
                    delete m_tree.u8Tree;
                    m_tree.u16Tree = newTree;
                } else if (newIdSize == IdSize::U20Bit) {
                    auto* newTree = new Tree20(*m_tree.u8Tree);
                    delete m_tree.u8Tree;
                    m_tree.u20Tree = newTree;
                }
                break;
            case IdSize::U16Bit:
                if (newIdSize == IdSize::U8Bit) {
                    auto* newTree = new Tree8(*m_tree.u16Tree);
                    delete m_tree.u16Tree;
                    m_tree.u8Tree = newTree;
                } else if (newIdSize == IdSize::U20Bit) {
                    auto* newTree = new Tree20(*m_tree.u16Tree);
                    delete m_tree.u16Tree;
                    m_tree.u20Tree = newTree;
                }
                break;
            case IdSize::U20Bit:
                if (newIdSize == IdSize::U8Bit) {
                    auto* newTree = new Tree8(*m_tree.u20Tree);
                    delete m_tree.u20Tree;
                    m_tree.u8Tree = newTree;
                } else if (newIdSize == IdSize::U16Bit) {
                    auto* newTree = new Tree16(*m_tree.u20Tree);
                    delete m_tree.u20Tree;
                    m_tree.u16Tree = newTree;
                }
                break;
        }
        m_idSize = newIdSize;
    }

    void clearPointers() {
        m_tree.u8Tree = nullptr;
        m_tree.u16Tree = nullptr;
        m_tree.u20Tree = nullptr;
    }
};