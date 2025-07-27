#pragma once

#include <ostream>
#include <istream>
#include <vector>
#include <unordered_map>

#include "VoxelData.h"
#include "common/Utils.h"

enum class VoxelIdSize : uint8_t {
    U8Bit = 8,
    U16Bit = 16,
    U20Bit = 20
};

template<VoxelIdSize Size>
class VoxelId {
public:
    using UintT = std::conditional_t<
        Size == VoxelIdSize::U8Bit,
        uint8_t,
        std::conditional_t<
            Size == VoxelIdSize::U16Bit,
            uint16_t,
            uint32_t
        >
    >;

    VoxelId() = default;
    VoxelId(UintT id) : m_id(id) {}

    bool isEmpty() const {
        return m_id == 0;
    }

    operator UintT() const {
        return m_id;
    }

private:
    UintT m_id{0};
};

template<VoxelIdSize Size>
class VoxelMapper {
    friend class VoxelMapper<VoxelIdSize::U8Bit>;
    friend class VoxelMapper<VoxelIdSize::U16Bit>;
    friend class VoxelMapper<VoxelIdSize::U20Bit>;
    using VoxelIdT = VoxelId<Size>;
    static constexpr size_t MAX_VOXELS = Utils::pow(static_cast<size_t>(2), static_cast<size_t>(Size));
    static constexpr VoxelData EMPTY_VOXEL{};
public:
    VoxelMapper() {
        m_voxels.push_back(EMPTY_VOXEL);
        m_voxelIdMap[EMPTY_VOXEL] = VoxelIdT(0);
    }

    template<VoxelIdSize OldVoxelIdSize>
    explicit VoxelMapper(const VoxelMapper<OldVoxelIdSize>& voxels)
        : m_voxels(voxels.m_voxels.begin(), voxels.m_voxels.end()) {
        assert(m_voxels.size() <= MAX_VOXELS && "VoxelMapper size exceeds maximum allowed size");
    }

    template<VoxelIdSize OldVoxelIdSize>
    VoxelMapper& operator=(const VoxelMapper<OldVoxelIdSize>& voxels) {
        m_voxels.assign(voxels.m_voxels.begin(), voxels.m_voxels.end());
        assert(m_voxels.size() <= MAX_VOXELS && "VoxelMapper size exceeds maximum allowed size");
        return *this;
    }

    [[nodiscard]] const VoxelData& getVoxelData(const VoxelIdT voxelId) const {
        if (voxelId < m_voxels.size()) {
            return m_voxels[voxelId];
        }
        return EMPTY_VOXEL;
    }

    std::pair<bool, VoxelIdT> tryGetVoxelId(const VoxelData &voxel) {
        if (auto it = m_voxelIdMap.find(voxel); it != m_voxelIdMap.end()) {
            return {true, it->second};
        }
        if (m_voxels.size() < MAX_VOXELS) {
            m_voxels.push_back(voxel);
            m_voxelIdMap[voxel] = static_cast<VoxelIdT>(m_voxels.size() - 1);
            return {true, static_cast<VoxelIdT>(m_voxels.size() - 1)};
        }
        return {false, VoxelIdT()};
    }

    [[nodiscard]] bool isEmpty() const {
        return m_voxels.empty();
    }

    void serialize(std::ostream& os) const {
        const auto size = static_cast<VoxelIdT>(m_voxels.size());
        os.write(reinterpret_cast<const char*>(&size), sizeof(VoxelIdT));
        os.write(reinterpret_cast<const char*>(m_voxels.data()), size * sizeof(VoxelData));
    }

    void deserialize(std::istream& is) {
        VoxelIdT size;
        is.read(reinterpret_cast<char*>(&size), sizeof(VoxelIdT));
        m_voxels.resize(size);
        is.read(reinterpret_cast<char*>(m_voxels.data()), size * sizeof(VoxelData));

        m_voxelIdMap.clear();
        for (size_t i = 0; i < m_voxels.size(); ++i) {
            m_voxelIdMap[m_voxels[i]] = static_cast<VoxelIdT>(i);
        }
    }

    static size_t getMaxSerializedSize() {
        return sizeof(VoxelData) * MAX_VOXELS;
    }

    [[nodiscard]] VoxelIdSize getMinVoxelIdSize() const {
        if (m_voxels.size() < VoxelMapper<VoxelIdSize::U8Bit>::MAX_VOXELS) {
            return VoxelIdSize::U8Bit;
        }
        if (m_voxels.size() < VoxelMapper<VoxelIdSize::U16Bit>::MAX_VOXELS) {
            return VoxelIdSize::U16Bit;
        }
        return VoxelIdSize::U20Bit;
    }

private:
    struct VoxelDataHasher {
        std::size_t operator()(const VoxelData& voxel) const {
            assert(sizeof(VoxelData) == sizeof(uint32_t) && "VoxelData must be 4 bytes for hashing");
            return std::hash<uint32_t>()(*reinterpret_cast<const uint32_t*>(&voxel));
        }
    };
    std::vector<VoxelData> m_voxels;
    std::unordered_map<VoxelData, VoxelIdT, VoxelDataHasher> m_voxelIdMap;
};
