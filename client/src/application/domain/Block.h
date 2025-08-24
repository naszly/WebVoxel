#pragma once
#include "BlockId.h"
#include "BlockLightInfo.h"

class Block {
public:
    explicit Block(const BlockId id) : m_id(id) {}
    bool emitsLight() const;
    BlockLightInfo getLightInfo() const;
    BlockId id() const { return m_id; }

private:
    BlockId m_id;
};
