#include "BlockTextureManager.h"

BlockTextureManager::BlockTextureManager(const std::vector<const char*>& textureFiles,
                                         const WebGpuContext& webGpuContext) {
    m_textureArray = std::make_unique<TextureArray>(webGpuContext, 16, 16, textureFiles.size());

    m_textureArray->loadTexturesRgba(textureFiles);

    for (size_t i = 0; i < textureFiles.size(); ++i) {
        m_textureNameMap[textureFiles[i]] = static_cast<TextureId>(i);
    }

    m_blockTextures.resize(static_cast<size_t>(BlockId::Count));
}

size_t BlockTextureManager::blockIdToIndex(const BlockId& block) const {
    const auto index = static_cast<size_t>(block);
    assert(index < m_blockTextures.size());
    return index;
}

TextureId BlockTextureManager::textureNameToId(const char* textureName) const {
    return m_textureNameMap.at(textureName);
}

void BlockTextureManager::setTextureForEastFace(const BlockId& block, const TextureId textureId) {
    const size_t index = blockIdToIndex(block);
    m_blockTextures[index].eastTextureId = textureId;
}

void BlockTextureManager::setTextureForTopFace(const BlockId& block, const TextureId textureId) {
    const size_t index = blockIdToIndex(block);
    m_blockTextures[index].topTextureId = textureId;
}

void BlockTextureManager::setTextureForNorthFace(const BlockId& block, const TextureId textureId) {
    const size_t index = blockIdToIndex(block);
    m_blockTextures[index].northTextureId = textureId;
}

void BlockTextureManager::setTextureForWestFace(const BlockId& block, const TextureId textureId) {
    const size_t index = blockIdToIndex(block);
    m_blockTextures[index].westTextureId = textureId;
}

void BlockTextureManager::setTextureForBottomFace(const BlockId& block, const TextureId textureId) {
    const size_t index = blockIdToIndex(block);
    m_blockTextures[index].bottomTextureId = textureId;
}

void BlockTextureManager::setTextureForSouthFace(const BlockId& block, const TextureId textureId) {
    const size_t index = blockIdToIndex(block);
    m_blockTextures[index].southTextureId = textureId;
}

void BlockTextureManager::initializeStorageBuffer(const WebGpuContext& webGpuContext) {
    m_storageBufferManager = std::make_unique<StorageBufferManager>(
           webGpuContext.getDevice(),
           wgpuDeviceGetQueue(webGpuContext.getDevice()),
           m_blockTextures.data(),
           m_blockTextures.size() * sizeof(BlockTextures)
       );
}

WGPUBuffer BlockTextureManager::getTextureIdsBuffer() const {
    return m_storageBufferManager->getBuffer();
}

uint64_t BlockTextureManager::getTextureIdsBufferSize() const {
    return m_storageBufferManager->getBufferSize();
}

WGPUTextureView BlockTextureManager::getTextureArrayView() const {
    return m_textureArray->getTextureView();
}

BlockTextureManagerBuilder::BlockTextureManagerBuilder(const std::vector<const char*>& textureFiles, const WebGpuContext& webGpuContext)
    : m_textureFiles(textureFiles), m_webGpuContext(webGpuContext) {}

BlockTextureManagerBuilder& BlockTextureManagerBuilder::setTextureForAllFaces(const BlockId& block, const char* texture) {
    m_ops.emplace_back([block, texture](BlockTextureManager& manager) {
        const TextureId textureId = manager.textureNameToId(texture);
        manager.setTextureForEastFace(block, textureId);
        manager.setTextureForTopFace(block, textureId);
        manager.setTextureForNorthFace(block, textureId);
        manager.setTextureForWestFace(block, textureId);
        manager.setTextureForBottomFace(block, textureId);
        manager.setTextureForSouthFace(block, textureId);
    });
    return *this;
}

BlockTextureManagerBuilder& BlockTextureManagerBuilder::setTextureForSideFaces(const BlockId& block, const char* texture) {
    m_ops.emplace_back([block, texture](BlockTextureManager& manager) {
        const TextureId textureId = manager.textureNameToId(texture);
        manager.setTextureForEastFace(block, textureId);
        manager.setTextureForNorthFace(block, textureId);
        manager.setTextureForWestFace(block, textureId);
        manager.setTextureForSouthFace(block, textureId);
    });
    return *this;
}

BlockTextureManagerBuilder& BlockTextureManagerBuilder::setTextureForTopFace(const BlockId& block, const char* texture) {
    m_ops.emplace_back([block, texture](BlockTextureManager& manager) {
        const TextureId textureId = manager.textureNameToId(texture);
        manager.setTextureForTopFace(block, textureId);
    });
    return *this;
}

BlockTextureManagerBuilder& BlockTextureManagerBuilder::setTextureForBottomFace(const BlockId& block, const char* texture) {
    m_ops.emplace_back([block, texture](BlockTextureManager& manager) {
        const TextureId textureId = manager.textureNameToId(texture);
        manager.setTextureForBottomFace(block, textureId);
    });
    return *this;
}

std::unique_ptr<BlockTextureManager> BlockTextureManagerBuilder::build() const {
    auto manager = std::unique_ptr<BlockTextureManager>(new BlockTextureManager(m_textureFiles, m_webGpuContext));
    for (auto& op : m_ops) {
        op(*manager);
    }
    manager->initializeStorageBuffer(m_webGpuContext);
    return manager;
}
