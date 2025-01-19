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

    static ChunkVertexBuffer createChunkVertexBuffer(const glm::ivec3 position, const ChunkBitmap& bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)>& getVoxel) {
        ChunkVertexBuffer vertexBuffer;

        static std::vector<VertexData> points;
        points.clear();

        getVertices(bitmap, getVoxel, points);

        const auto device = GetWebGPUContext().getDevice();
        const auto queue = wgpuDeviceGetQueue(device);

        const auto chunkPosition = glm::vec4(position, 0.0f);

        const size_t bufferSize = points.size() * sizeof(VertexData) + sizeof(chunkPosition);

        WGPUBufferDescriptor descriptor{};
        descriptor.size = bufferSize;
        descriptor.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
        descriptor.mappedAtCreation = false;
        descriptor.label = "Chunk Vertex Buffer";

        vertexBuffer.buffer = wgpuDeviceCreateBuffer(device, &descriptor);
        vertexBuffer.vertexCount = points.size();

        static std::vector<uint8_t> data;
        if (data.size() < bufferSize) {
            data.resize(bufferSize);
        }

        memcpy(data.data(), points.data(), points.size() * sizeof(VertexData));
        memcpy(data.data() + points.size() * sizeof(VertexData), &chunkPosition, sizeof(chunkPosition));

        wgpuQueueWriteBuffer(queue, vertexBuffer.buffer, 0, data.data(), bufferSize);

        return vertexBuffer;
    }

    static void getVertices(const ChunkBitmap& bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)>& getVoxel, std::vector<VertexData>& vertices) {

        for (uint32_t x = 1; x < BITMAP_SIZE - 1; x++) {
            for (uint32_t y = 1; y < BITMAP_SIZE - 1; y++) {
                for (uint32_t z = 1; z < BITMAP_SIZE - 1; z++) {
                    const uint32_t i = x * BITMAP_SIZE * BITMAP_SIZE + y * BITMAP_SIZE + z;

                    if (!bitmap.test(i)) {
                        continue;
                    }

                    const bool isVisible =
                        !(bitmap.test(i-1) && bitmap.test(i+1) &&
                          bitmap.test(i-BITMAP_SIZE) && bitmap.test(i+BITMAP_SIZE) &&
                          bitmap.test(i-BITMAP_SIZE*BITMAP_SIZE) && bitmap.test(i+BITMAP_SIZE*BITMAP_SIZE));

                    if (isVisible) {
                        const auto& voxel = getVoxel(x-1, y-1, z-1);
                        vertices.emplace_back(x-1, y-1, z-1, 1, voxel);
                    }
                }
            }
        }
    }
};