#pragma once

#include "System.h"

#include <glm/glm.hpp>

#include "common/datastructures/HashMap.h"
#include "../graphics/VertexData.h"
#include "application/domain/BlockLightInfo.h"

class RendererSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    explicit RendererSystem() : System() {}

    [[nodiscard]] bool getLighting() const {
        return m_lighting;
    }

    void setLighting(bool lighting);

    [[nodiscard]] bool getFog() const {
        return m_fog;
    }

    void setFog(bool fog);

    void exportTimestamps() const;

private:
    bool m_lighting = true;
    bool m_fog = true;
    const int m_sampleCount = 1;

    struct Uniforms {
        glm::mat4 projectionViewMatrix;
        glm::mat4 inverseProjectionViewMatrix;
        glm::vec3 cameraPosition;
        float fov;
        glm::vec2 viewportSize;
        float nearPlane;
        float farPlane;
    };
    static_assert(sizeof(Uniforms) % 16 == 0);

    WGPUTexture m_depthTexture{};
    WGPUTextureView m_depthTextureView{};
    WGPUTexture m_multisampleColorTexture{};
    WGPUTextureView m_multisampleColorTextureView{};
    unsigned int m_viewportWidth{}, m_viewportHeight{};

    WGPUQueue m_queue{};
    WGPURenderPipeline m_renderPipeline{};
    WGPUBuffer m_billboardVertexBuffer{};
    WGPUBuffer m_billboardIndexBuffer{};
    uint32_t m_billboardIndexCount{};
    WGPUBuffer m_uniformBuffer{};
    Uniforms m_uniformData{};
    WGPUBindGroup m_uniformBindGroup{};

    struct ChunkVertexBuffer {
        WGPUBuffer buffer{nullptr};
        size_t vertexCount{0};
    };

    HashMap<glm::ivec3, ChunkVertexBuffer> m_chunkVertexBuffers;

    WGPUQuerySet m_querySet{};
    WGPUBuffer m_queryResolveBuffer{};
    WGPUBuffer m_queryReadBuffer{};
    const uint64_t m_queryReadBufferCapacity = 524288 * sizeof(uint64_t);
    uint64_t m_queryReadBufferSize{};

    void createRenderPipeline();
    void createDepthTexture();
    static std::string loadShader(const char* filename);
    void initializeBuffers();

    std::vector<std::reference_wrapper<Chunk>> collectDirtyChunks(World& world, const glm::ivec3& playerChunk) const;
    void processDirtyChunks(const World& world, const std::vector<std::reference_wrapper<Chunk>>& dirtyChunks);
    void removeFarChunkBuffers(const glm::ivec3& playerChunk);

    void exportTimestampsInternal() const;

    [[nodiscard]] static bool hasAllNeighbours(const World &world, const Chunk& chunk);

    [[nodiscard]] static bool testBitmaps(const ChunkNeighborhood& neighborChunks, uint32_t x, uint32_t y, uint32_t z);

    [[nodiscard]] ChunkVertexBuffer createChunkVertexBuffer(const ChunkNeighborhood& neighborChunks) const;

    static void getVertices(const ChunkNeighborhood& neighborChunks, std::vector<VertexData> &vertices);

    static AmbientOcclusion getAmbientOcclusion(const ChunkNeighborhood& neighborChunks, uint32_t x, uint32_t y, uint32_t z);

    // using unique_ptr to allocate on the heap to avoid stack overflow
    using LightMap = std::array<std::array<std::array<BlockLightInfo, Chunk::WIDTH*3>, Chunk::WIDTH*3>, Chunk::WIDTH*3>;
    using LightMapPtr = std::unique_ptr<LightMap>;

    static LightMap& propagateLight(const ChunkNeighborhood& neighborChunks,
                                    const std::vector<Chunk::LightSource>& lights);
};