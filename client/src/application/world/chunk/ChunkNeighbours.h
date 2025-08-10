#pragma once

class Chunk;

class ChunkNeighbours {
public:
    ChunkNeighbours(const Chunk *xMinus, const Chunk *xPlus, const Chunk *yMinus, const Chunk *yPlus,
        const Chunk *zMinus, const Chunk *zPlus)
        : xMinus(xMinus),
          xPlus(xPlus),
          yMinus(yMinus),
          yPlus(yPlus),
          zMinus(zMinus),
          zPlus(zPlus) {}

    const Chunk* xMinus{nullptr};
    const Chunk* xPlus{nullptr};
    const Chunk* yMinus{nullptr};
    const Chunk* yPlus{nullptr};
    const Chunk* zMinus{nullptr};
    const Chunk* zPlus{nullptr};

    [[nodiscard]] bool hasAllNeighbours() const;

    [[nodiscard]] bool anyNeighbourDirty() const;
};

class ExtendedChukNeighbours : public ChunkNeighbours {
public:
    ExtendedChukNeighbours(const Chunk *xMinus, const Chunk *xPlus, const Chunk *yMinus, const Chunk *yPlus,
        const Chunk *zMinus, const Chunk *zPlus, const Chunk *xMinusYMinus, const Chunk *xMinusYPlus,
        const Chunk *xMinusZMinus, const Chunk *xMinusZPlus, const Chunk *xPlusYMinus,
        const Chunk *xPlusYPlus, const Chunk *xPlusZMinus, const Chunk *xPlusZPlus,
        const Chunk *yMinusZMinus, const Chunk *yMinusZPlus, const Chunk *yPlusZMinus,
        const Chunk *yPlusZPlus, const Chunk *xMinusYMinusZMinus, const Chunk *xMinusYMinusZPlus,
        const Chunk *xMinusYPlusZMinus, const Chunk *xMinusYPlusZPlus, const Chunk *xPlusYMinusZMinus,
        const Chunk *xPlusYMinusZPlus, const Chunk *xPlusYPlusZMinus, const Chunk *xPlusYPlusZPlus)
        : ChunkNeighbours(xMinus, xPlus, yMinus, yPlus, zMinus, zPlus),
          xMinusYMinus(xMinusYMinus),
          xMinusYPlus(xMinusYPlus),
          xMinusZMinus(xMinusZMinus),
          xMinusZPlus(xMinusZPlus),
          xPlusYMinus(xPlusYMinus),
          xPlusYPlus(xPlusYPlus),
          xPlusZMinus(xPlusZMinus),
          xPlusZPlus(xPlusZPlus),
          yMinusZMinus(yMinusZMinus),
          yMinusZPlus(yMinusZPlus),
          yPlusZMinus(yPlusZMinus),
          yPlusZPlus(yPlusZPlus),
          xMinusYMinusZMinus(xMinusYMinusZMinus),
          xMinusYMinusZPlus(xMinusYMinusZPlus),
          xMinusYPlusZMinus(xMinusYPlusZMinus),
          xMinusYPlusZPlus(xMinusYPlusZPlus),
          xPlusYMinusZMinus(xPlusYMinusZMinus),
          xPlusYMinusZPlus(xPlusYMinusZPlus),
          xPlusYPlusZMinus(xPlusYPlusZMinus),
          xPlusYPlusZPlus(xPlusYPlusZPlus) {}

    const Chunk* xMinusYMinus{nullptr};
    const Chunk* xMinusYPlus{nullptr};
    const Chunk* xMinusZMinus{nullptr};
    const Chunk* xMinusZPlus{nullptr};
    const Chunk* xPlusYMinus{nullptr};
    const Chunk* xPlusYPlus{nullptr};
    const Chunk* xPlusZMinus{nullptr};
    const Chunk* xPlusZPlus{nullptr};
    const Chunk* yMinusZMinus{nullptr};
    const Chunk* yMinusZPlus{nullptr};
    const Chunk* yPlusZMinus{nullptr};
    const Chunk* yPlusZPlus{nullptr};
    const Chunk* xMinusYMinusZMinus{nullptr};
    const Chunk* xMinusYMinusZPlus{nullptr};
    const Chunk* xMinusYPlusZMinus{nullptr};
    const Chunk* xMinusYPlusZPlus{nullptr};
    const Chunk* xPlusYMinusZMinus{nullptr};
    const Chunk* xPlusYMinusZPlus{nullptr};
    const Chunk* xPlusYPlusZMinus{nullptr};
    const Chunk* xPlusYPlusZPlus{nullptr};

    [[nodiscard]] bool hasAllNeighbours() const;

    [[nodiscard]] bool anyNeighbourDirty() const;
};