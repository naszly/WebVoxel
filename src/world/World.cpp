#include "World.h"

#include <cstdlib>

World::World() {
    m_Chunk.fill([](int x, int y, int z) {
        if (rand() % 100 > 0) {
            return Voxel();
        }
        Voxel voxel;
        voxel.r = rand() % 256;
        voxel.g = rand() % 256;
        voxel.b = rand() % 256;
        return voxel;
    });
}
