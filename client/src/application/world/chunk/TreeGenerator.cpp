#include "TreeGenerator.h"

void TreeGenerator::generateBush(int anchorX, int anchorZ, int surfaceHeight, const SetVoxelFunc& setVoxel) {
    for (int x = -1; x <= 1; ++x) {
        for (int z = -1; z <= 1; ++z) {
            if (std::abs(x) + std::abs(z) <= 1) {
                setVoxel(BlockId::OakLeaves, anchorX + x, surfaceHeight + 1, anchorZ + z);
            }
        }
    }
    setVoxel(BlockId::OakLeaves, anchorX, surfaceHeight + 2, anchorZ);
}

void TreeGenerator::generateTree(int anchorX, int anchorZ, int surfaceHeight, 
                                const WorldGenerator::Vegetation& vegetation,
                                const SetVoxelFunc& setVoxel) {
    switch (vegetation.variant) {
    case 0:
        generateWideOak(anchorX, anchorZ, surfaceHeight, vegetation.height, setVoxel);
        break;
    case 1:
        generateTallOak(anchorX, anchorZ, surfaceHeight, vegetation.height, setVoxel);
        break;
    case 2:
        generateBranchingOak(anchorX, anchorZ, surfaceHeight, vegetation.height, setVoxel);
        break;
    case 3:
        generateCornicalOak(anchorX, anchorZ, surfaceHeight, vegetation.height, setVoxel);
        break;
    }
}

void TreeGenerator::generateWideOak(int anchorX, int anchorZ, int surfaceHeight, 
                                   uint8_t height, const SetVoxelFunc& setVoxel) {
    // Trunk
    for (int y = 1; y <= height; ++y) {
        setVoxel(BlockId::OakLog, anchorX, surfaceHeight + y, anchorZ);
    }
    const int canopyY = surfaceHeight + height;
    
    // Large canopy
    for (int y = -3; y <= 1; ++y) {
        const int radius = y == 1 ? 1 : (y == 0 ? 2 : 3);
        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                if (std::abs(x) == radius && std::abs(z) == radius &&
                    ((anchorX + anchorZ + x + z + y) & 1) != 0) continue;
                setVoxel(BlockId::OakLeaves, anchorX + x, canopyY + y, anchorZ + z);
            }
        }
    }
    setVoxel(BlockId::OakLeaves, anchorX, canopyY + 2, anchorZ);
}

void TreeGenerator::generateTallOak(int anchorX, int anchorZ, int surfaceHeight, 
                                  uint8_t height, const SetVoxelFunc& setVoxel) {
    // Trunk
    for (int y = 1; y <= height; ++y) {
        setVoxel(BlockId::OakLog, anchorX, surfaceHeight + y, anchorZ);
    }
    const int canopyY = surfaceHeight + height;
    
    // Narrow canopy
    for (int y = -1; y <= 0; ++y) {
        const int radius = y == 0 ? 1 : 2;
        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                if (std::abs(x) == radius && std::abs(z) == radius &&
                    ((anchorX + anchorZ + x + z + y) & 1) != 0) continue;
                setVoxel(BlockId::OakLeaves, anchorX + x, canopyY + y, anchorZ + z);
            }
        }
    }
    setVoxel(BlockId::OakLeaves, anchorX, canopyY + 1, anchorZ);
}

void TreeGenerator::generateBranchingOak(int anchorX, int anchorZ, int surfaceHeight, 
                                        uint8_t height, const SetVoxelFunc& setVoxel) {
    // Pick a random side for the branch based on position
    uint32_t hash = static_cast<uint32_t>(anchorX) * 73856093u ^ static_cast<uint32_t>(anchorZ) * 19349663u;
    const int branchSide = hash % 4;  // 0=+x, 1=-x, 2=+z, 3=-z
    
    const int branchStart = height / 2 + hash % 3;
    
    // Trunk with single branch on random side
    for (int y = 1; y <= height; ++y) {
        setVoxel(BlockId::OakLog, anchorX, surfaceHeight + y, anchorZ);
        // Single branch extending outward
        if (y >= branchStart && y < height - 2) {
            const int branchLength = y - branchStart + 1;
            switch (branchSide) {
            case 0:  // +x branch
                setVoxel(BlockId::OakLog, anchorX + branchLength, surfaceHeight + y, anchorZ);
                break;
            case 1:  // -x branch
                setVoxel(BlockId::OakLog, anchorX - branchLength, surfaceHeight + y, anchorZ);
                break;
            case 2:  // +z branch
                setVoxel(BlockId::OakLog, anchorX, surfaceHeight + y, anchorZ + branchLength);
                break;
            case 3:  // -z branch
                setVoxel(BlockId::OakLog, anchorX, surfaceHeight + y, anchorZ - branchLength);
                break;
            }
        }
    }
    const int canopyY = surfaceHeight + height;
    
    // Canopy around main trunk and branch tip
    for (int y = -1; y <= 1; ++y) {
        const int radius = y == 1 ? 1 : 2;
        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                setVoxel(BlockId::OakLeaves, anchorX + x, canopyY + y, anchorZ + z);
            }
        }
    }
}

void TreeGenerator::generateCornicalOak(int anchorX, int anchorZ, int surfaceHeight, 
                                       uint8_t height, const SetVoxelFunc& setVoxel) {
    // Trunk
    for (int y = 1; y <= height; ++y) {
        setVoxel(BlockId::OakLog, anchorX, surfaceHeight + y, anchorZ);
    }
    const int canopyY = surfaceHeight + height;
    
    // Conical tapered canopy
    for (int y = -2; y <= 0; ++y) {
        const int radius = y == 0 ? 1 : (y == -1 ? 2 : 3);
        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                if (std::abs(x) + std::abs(z) > radius) continue;
                setVoxel(BlockId::OakLeaves, anchorX + x, canopyY + y, anchorZ + z);
            }
        }
    }
    setVoxel(BlockId::OakLeaves, anchorX, canopyY + 1, anchorZ);
}

void TreeGenerator::generateBirchTree(int anchorX, int anchorZ, int surfaceHeight,
                                       uint8_t height, const SetVoxelFunc& setVoxel) {
    // Slim trunk
    for (int y = 1; y <= height; ++y) {
        setVoxel(BlockId::BirchLog, anchorX, surfaceHeight + y, anchorZ);
    }
    const int canopyY = surfaceHeight + height;

    // Small round canopy: radius 2 at base, radius 1 at top
    for (int y = -1; y <= 1; ++y) {
        const int radius = (y == 1) ? 1 : 2;
        for (int x = -radius; x <= radius; ++x) {
            for (int z = -radius; z <= radius; ++z) {
                if (std::abs(x) + std::abs(z) > radius + 1) continue;
                if (std::abs(x) == radius && std::abs(z) == radius) continue;
                setVoxel(BlockId::BirchLeaves, anchorX + x, canopyY + y, anchorZ + z);
            }
        }
    }
    setVoxel(BlockId::BirchLeaves, anchorX, canopyY + 2, anchorZ);
}
