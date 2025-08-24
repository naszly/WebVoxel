#include "Block.h"
#include "BlockId.h"
#include "BlockLightInfo.h"
#include <unordered_map>

typedef std::unordered_map<BlockId, BlockLightInfo> BlockLightMap;
static const BlockLightMap BLOCK_LIGHT_INFO_MAP = {
    { BlockId::Sunstone, BlockLightInfo{ 1.0f } }
};

bool Block::emitsLight() const {
    const auto it = BLOCK_LIGHT_INFO_MAP.find(m_id);
    return it != BLOCK_LIGHT_INFO_MAP.end();
}

BlockLightInfo Block::getLightInfo() const {
    if (const auto it = BLOCK_LIGHT_INFO_MAP.find(m_id); it != BLOCK_LIGHT_INFO_MAP.end()) {
        return it->second;
    }
    return BlockLightInfo{ 0.0f };
}

