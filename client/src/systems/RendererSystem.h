#pragma once

#include "System.h"

#include <glm/glm.hpp>

class RendererSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    explicit RendererSystem(const bool ambientOcclusion = false) : System(), m_ambient_occlusion(ambientOcclusion) {}

    [[nodiscard]] bool getAmbientOcclusion() const {
        return m_ambient_occlusion;
    }

    void setAmbientOcclusion(bool ambientOcclusion);

private:
    bool m_ambient_occlusion;
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

    WGPUTexture m_DepthTexture{};
    WGPUTextureView m_DepthTextureView{};
    WGPUTexture m_MultisampleColorTexture{};
    WGPUTextureView m_MultisampleColorTextureView{};
    unsigned int m_ViewportWidth{}, m_ViewportHeight{};

    WGPUQueue m_Queue{};
    WGPURenderPipeline m_RenderPipeline{};
    WGPUBuffer m_BillboardVertexBuffer{};
    WGPUBuffer m_BillboardIndexBuffer{};
    uint32_t m_BillboardIndexCount{};
    WGPUBuffer m_UniformBuffer{};
    Uniforms m_UniformData{};
    WGPUBindGroup m_UniformBindGroup{};

    struct ChunkVertexBuffer {
        WGPUBuffer buffer{nullptr};
        size_t vertexCount{0};
    };

    std::unordered_map<glm::ivec3, ChunkVertexBuffer> m_ChunkVertexBuffers;

    void createRenderPipeline();
    void createDepthTexture();
    static std::string LoadShader(const char* filename);
    void InitializeBuffers();

    static constexpr float FOV = glm::radians(66.0);

    static constexpr uint32_t BITMAP_SIZE = Chunk::SIZE + 2;
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