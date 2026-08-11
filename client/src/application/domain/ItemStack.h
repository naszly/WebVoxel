#pragma once

#include "BlockId.h"

struct ItemStack {
    BlockId blockId{BlockId::Air};
    int count{0};

    [[nodiscard]] bool isEmpty() const { return count <= 0 || blockId == BlockId::Air; }

    static ItemStack empty() { return {BlockId::Air, 0}; }
    static ItemStack of(BlockId block, int count = 1) { return {block, count}; }
};
