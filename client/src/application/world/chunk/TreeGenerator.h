#pragma once

#include "application/domain/BlockId.h"
#include "application/world/WorldGenerator.h"

class TreeGenerator {
public:
    using SetVoxelFunc = std::function<void(BlockId, int, int, int)>;

    static void generateBush(int anchorX, int anchorZ, int surfaceHeight, const SetVoxelFunc& setVoxel);

    static void generateTree(int anchorX, int anchorZ, int surfaceHeight, 
                           const WorldGenerator::Vegetation& vegetation, 
                           const SetVoxelFunc& setVoxel);

private:
    static void generateWideOak(int anchorX, int anchorZ, int surfaceHeight, 
                               uint8_t height, const SetVoxelFunc& setVoxel);
    static void generateTallOak(int anchorX, int anchorZ, int surfaceHeight, 
                              uint8_t height, const SetVoxelFunc& setVoxel);
    static void generateBranchingOak(int anchorX, int anchorZ, int surfaceHeight, 
                                    uint8_t height, const SetVoxelFunc& setVoxel);
    static void generateCornicalOak(int anchorX, int anchorZ, int surfaceHeight, 
                                   uint8_t height, const SetVoxelFunc& setVoxel);
};
