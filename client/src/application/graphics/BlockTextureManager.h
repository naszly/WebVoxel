#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "resources/StorageBuffer.h"
#include "resources/TextureArray.h"
#include "application/domain/BlockId.h"
#include "common/datastructures/HashMap.h"

using TextureId = uint32_t;

enum class BlockFace : uint8_t {
    East = 0,
    Top = 1,
    North = 2,
    West = 3,
    Bottom = 4,
    South = 5
};

struct BlockTextures {
    TextureId eastTextureId;
    TextureId topTextureId;
    TextureId northTextureId;
    TextureId westTextureId;
    TextureId bottomTextureId;
    TextureId southTextureId;
};

class BlockTextureManager {
    friend class BlockTextureManagerBuilder;
    BlockTextureManager(const std::vector<const char*>& textureFiles, const WebGpuContext& webGpuContext);

    [[nodiscard]] size_t blockIdToIndex(const BlockId& block) const;
    [[nodiscard]] TextureId textureNameToId(const char* textureName) const;

    void setTextureForEastFace(const BlockId& block, TextureId textureId);
    void setTextureForTopFace(const BlockId& block, TextureId textureId);
    void setTextureForNorthFace(const BlockId& block, TextureId textureId);
    void setTextureForWestFace(const BlockId& block, TextureId textureId);
    void setTextureForBottomFace(const BlockId& block, TextureId textureId);
    void setTextureForSouthFace(const BlockId& block, TextureId textureId);

    void initializeStorageBuffer(const WebGpuContext& webGpuContext);

public:
    [[nodiscard]] WGPUBuffer getTextureIdsBuffer() const;
    [[nodiscard]] uint64_t getTextureIdsBufferSize() const;
    [[nodiscard]] WGPUTextureView getTextureArrayView() const;

private:
    std::vector<BlockTextures> m_blockTextures;
    HashMap<const char*, TextureId> m_textureNameMap;
    std::unique_ptr<TextureArray> m_textureArray;
    std::unique_ptr<StorageBuffer> m_storageBuffer;
};

class BlockTextureManagerBuilder {
public:
    BlockTextureManagerBuilder(const std::vector<const char*>& textureFiles, const WebGpuContext& webGpuContext);

    BlockTextureManagerBuilder& setTextureForAllFaces(const BlockId& block, const char* texture);
    BlockTextureManagerBuilder& setTextureForSideFaces(const BlockId& block, const char* texture);
    BlockTextureManagerBuilder& setTextureForTopFace(const BlockId& block, const char* texture);
    BlockTextureManagerBuilder& setTextureForBottomFace(const BlockId& block, const char* texture);

    [[nodiscard]] std::unique_ptr<BlockTextureManager> build() const;

private:
    std::vector<const char*> m_textureFiles;
    const WebGpuContext& m_webGpuContext;
    std::vector<std::function<void(BlockTextureManager&)>> m_ops;
};
