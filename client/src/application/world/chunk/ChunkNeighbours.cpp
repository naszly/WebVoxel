#include "ChunkNeighbours.h"

#include "Chunk.h"

bool ChukNeighbours::hasAllNeighbours() const {
    return xMinus && xPlus && yMinus && yPlus && zMinus && zPlus &&
        xMinusYMinus && xMinusYPlus && xMinusZMinus && xMinusZPlus &&
        xPlusYMinus && xPlusYPlus && xPlusZMinus && xPlusZPlus &&
        yMinusZMinus && yMinusZPlus && yPlusZMinus && yPlusZPlus &&
        xMinusYMinusZMinus && xMinusYMinusZPlus && xMinusYPlusZMinus && xMinusYPlusZPlus &&
        xPlusYMinusZMinus && xPlusYMinusZPlus && xPlusYPlusZMinus && xPlusYPlusZPlus;
}

bool ChukNeighbours::anyNeighbourDirty() const {
    return (xMinus && xMinus->isGpuBufferDirty()) ||
        (xPlus && xPlus->isGpuBufferDirty()) ||
        (yMinus && yMinus->isGpuBufferDirty()) ||
        (yPlus && yPlus->isGpuBufferDirty()) ||
        (zMinus && zMinus->isGpuBufferDirty()) ||
        (zPlus && zPlus->isGpuBufferDirty()) ||
        (xMinusYMinus && xMinusYMinus->isGpuBufferDirty()) ||
        (xMinusYPlus && xMinusYPlus->isGpuBufferDirty()) ||
        (xMinusZMinus && xMinusZMinus->isGpuBufferDirty()) ||
        (xMinusZPlus && xMinusZPlus->isGpuBufferDirty()) ||
        (xPlusYMinus && xPlusYMinus->isGpuBufferDirty()) ||
        (xPlusYPlus && xPlusYPlus->isGpuBufferDirty()) ||
        (xPlusZMinus && xPlusZMinus->isGpuBufferDirty()) ||
        (xPlusZPlus && xPlusZPlus->isGpuBufferDirty()) ||
        (yMinusZMinus && yMinusZMinus->isGpuBufferDirty()) ||
        (yMinusZPlus && yMinusZPlus->isGpuBufferDirty()) ||
        (yPlusZMinus && yPlusZMinus->isGpuBufferDirty()) ||
        (yPlusZPlus && yPlusZPlus->isGpuBufferDirty()) ||
        (xMinusYMinusZMinus && xMinusYMinusZMinus->isGpuBufferDirty()) ||
        (xMinusYMinusZPlus && xMinusYMinusZPlus->isGpuBufferDirty()) ||
        (xMinusYPlusZMinus && xMinusYPlusZMinus->isGpuBufferDirty()) ||
        (xMinusYPlusZPlus && xMinusYPlusZPlus->isGpuBufferDirty()) ||
        (xPlusYMinusZMinus && xPlusYMinusZMinus->isGpuBufferDirty()) ||
        (xPlusYMinusZPlus && xPlusYMinusZPlus->isGpuBufferDirty()) ||
        (xPlusYPlusZMinus && xPlusYPlusZMinus->isGpuBufferDirty()) ||
        (xPlusYPlusZPlus && xPlusYPlusZPlus->isGpuBufferDirty());
}
