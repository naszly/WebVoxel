#pragma once

#include "System.h"

#include <glm/glm.hpp>

#include "common/datastructures/HashMap.h"
#include "../graphics/VertexData.h"
#include "../graphics/StorageBufferManager.h"

class RendererSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    explicit RendererSystem() : System() {}

    [[nodiscard]] bool getAmbientOcclusion() const {
        return m_ambientOcclusion;
    }

    void setAmbientOcclusion(bool ambientOcclusion);

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
    bool m_ambientOcclusion = true;
    bool m_lighting = true;
    bool m_fog = true;
    const int m_sampleCount = 1;

    struct Uniforms {
        glm::mat4 transposedProjectionViewMatrix;
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

    void exportTimestampsInternal() const;

    static constexpr float FOV = glm::radians(66.0);

    static constexpr uint32_t BITMAP_SIZE = Chunk::WIDTH + 2;
    using ChunkBitmap = Bitmap<BITMAP_SIZE * BITMAP_SIZE * BITMAP_SIZE>;

    std::optional<ChunkBitmap> getBitmap(const World &world, const Chunk& chunk) const;

    template<typename VertexT>
    static ChunkVertexBuffer createChunkVertexBuffer(glm::ivec3 position, const ChunkBitmap& bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)>& getVoxel);

    template<typename VertexT>
    static void getVertices(const ChunkBitmap &bitmap,
                                     const std::function<VoxelData(uint32_t, uint32_t, uint32_t)> &getVoxel,
                                     std::vector<VertexT> &vertices);

    static AmbientOcclusion getAmbientOcclusion(const ChunkBitmap& bitmap, uint32_t x, uint32_t y, uint32_t z);
};