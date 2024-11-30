#pragma once

#include "System.h"

#include <glm/glm.hpp>

class RendererSystem : public System {
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

    template<uint32_t SIZE>
    ChunkVertexBuffer createChunkVertexBuffer(const glm::ivec3 position, const Bitmap<SIZE*SIZE*SIZE>& bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)>& getVoxel) {
        ChunkVertexBuffer vertexBuffer;

        const std::vector<VertexData> points = getVertices<SIZE>(bitmap, getVoxel);

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

        std::vector<uint8_t> data(bufferSize);
        memcpy(data.data(), points.data(), points.size() * sizeof(VertexData));
        memcpy(data.data() + points.size() * sizeof(VertexData), &chunkPosition, sizeof(chunkPosition));

        wgpuQueueWriteBuffer(queue, vertexBuffer.buffer, 0, data.data(), data.size());

        return vertexBuffer;
    }

    template<uint32_t SIZE>
    std::vector<VertexData> getVertices(const Bitmap<SIZE*SIZE*SIZE>& bitmap, const std::function<VoxelData(uint32_t, uint32_t, uint32_t)>& getVoxel) {
        std::vector<VertexData> vertices;

        for (uint32_t x = 1; x < SIZE - 1; x++) {
            for (uint32_t y = 1; y < SIZE - 1; y++) {
                for (uint32_t z = 1; z < SIZE - 1; z++) {
                    const uint32_t i = x * SIZE * SIZE + y * SIZE + z;

                    if (!bitmap.test(i)) {
                        continue;
                    }

                    const bool isVisible =
                        !(bitmap.test(i-1) && bitmap.test(i+1) &&
                          bitmap.test(i-SIZE) && bitmap.test(i+SIZE) &&
                          bitmap.test(i-SIZE*SIZE) && bitmap.test(i+SIZE*SIZE));

                    if (isVisible) {
                        const auto& voxel = getVoxel(x-1, y-1, z-1);
                        vertices.emplace_back(x-1, y-1, z-1, 1, voxel);
                    }
                }
            }
        }

        return vertices;
    }
};