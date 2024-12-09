#pragma once

#include "System.h"

#include <glm/glm.hpp>

class RendererSystem final : public System {
public:
    void initialize() override;
    void render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) override;
    void update(float dt) override;
    void onEvent(Event& event) override;

    RendererSystem() : System() {}

private:
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

    struct DynamicUniforms {
        glm::vec4 positionOffset;
    };
    static_assert(sizeof(DynamicUniforms) % 16 == 0);

    WGPUTexture depthTexture{};
    WGPUTextureView depthTextureView{};
    WGPUTexture multisampleColorTexture{};
    WGPUTextureView multisampleColorTextureView{};
    unsigned int  m_viewportWidth{}, m_viewportHeight{};

    WGPUQueue m_Queue{};
    WGPURenderPipeline m_RenderPipeline{};
    WGPUBuffer vertexBuffer{};
    uint32_t vertexCount{};
    WGPUBuffer indexBuffer{};
    uint32_t indexCount{};
    WGPUBuffer uniformBuffer{};
    Uniforms uniformData{};
    WGPUBuffer dynamicUniformBuffer{};
    DynamicUniforms dynamicUniformData{};
    WGPUBindGroup uniformBindGroup{};

    struct ChunkVertexBuffer {
        WGPUBuffer buffer{nullptr};
        size_t vertexCount{0};
    };

    std::unordered_map<glm::ivec3, ChunkVertexBuffer> m_ChunkVertexBuffers;

    void createRenderPipeline();
    void createDepthTexture();
    static std::string LoadShader(const char* filename);
    void InitializeBuffers();

    static constexpr uint32_t BITMAP_SIZE = Chunk::SIZE + 2;
    using ChunkBitmap = Bitmap<BITMAP_SIZE*BITMAP_SIZE*BITMAP_SIZE>;

    static ChunkVertexBuffer createChunkVertexBuffer(glm::ivec3 position, const ChunkBitmap& bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)>& getVoxel);

    static void getVertices(const ChunkBitmap& bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)>& getVoxel, std::vector<VertexData>& vertices);
};