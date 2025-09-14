#pragma once
#include "BlockId.h"
#include "BlockLightInfo.h"

class Block {
public:
    explicit Block(const BlockId id) : m_id(id) {}
    [[nodiscard]] bool emitsLight() const;
    [[nodiscard]] BlockLightInfo getLightInfo() const;
    [[nodiscard]] BlockId id() const { return m_id; }

private:
    BlockId m_id;
};
