#include "ShadowPipelineBuilder.h"

#include <array>
#include <cstddef>
#include "common/FileSystem.h"
#include "application/types/VertexData.h"
#include "application/world/chunk/Chunk.h"

ShadowPipelineBuilder::Artifacts ShadowPipelineBuilder::build(const WGPUDevice &device,
                                                              const WGPUBuffer &shadowUniformBuffer,
                                                              size_t chunkSize) {
    Artifacts out{};

    auto shaderBuf = FileSystem::readFileNative("shaders/shadow.wgsl");
    std::string shaderCode(shaderBuf.begin(), shaderBuf.end());

    WGPUShaderModuleDescriptor shaderDesc{};
    WGPUShaderSourceWGSL code{};
    code.chain.sType = WGPUSType_ShaderSourceWGSL;
    code.code = WGPUStringView{shaderCode.c_str(), shaderCode.size()};
    shaderDesc.nextInChain = &code.chain;
    auto shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    // Bind group layout (uniforms only)
    WGPUBindGroupLayoutEntry ubo{};
    ubo.binding = 0;
    ubo.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    ubo.buffer.type = WGPUBufferBindingType_Uniform;
    ubo.buffer.minBindingSize = wgpuBufferGetSize(shadowUniformBuffer);

    std::array layoutEntries{ubo};
    WGPUBindGroupLayoutDescriptor bglDesc{ .entryCount = layoutEntries.size(), .entries = layoutEntries.data() };
    auto bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    // Bind group
    WGPUBindGroupEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.buffer = shadowUniformBuffer;
    uboEntry.size = wgpuBufferGetSize(shadowUniformBuffer);

    std::array bgEntries{uboEntry};
    WGPUBindGroupDescriptor bgDesc{ .layout = bgl, .entryCount = bgEntries.size(), .entries = bgEntries.data() };
    out.bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    // Pipeline layout
    WGPUPipelineLayoutDescriptor plDesc{ .bindGroupLayoutCount = 1, .bindGroupLayouts = &bgl };
    auto pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUVertexBufferLayout voxelVb{};
    constexpr std::array voxelAttrs{
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = 0, .shaderLocation = 0 },
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = offsetof(VertexData, voxel), .shaderLocation = 1 },
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = offsetof(VertexData, ambientOcclusion), .shaderLocation = 3 },
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = offsetof(VertexData, pointLight), .shaderLocation = 4 },
    };
    voxelVb.attributeCount = voxelAttrs.size();
    voxelVb.attributes = voxelAttrs.data();
    voxelVb.arrayStride = sizeof(VertexData);
    voxelVb.stepMode = WGPUVertexStepMode_Instance;

    WGPUVertexBufferLayout chunkVb{};
    constexpr std::array chunkAttrs{ WGPUVertexAttribute{ .format = WGPUVertexFormat_Float32x4, .offset = 0, .shaderLocation = 2 } };
    chunkVb.attributeCount = chunkAttrs.size();
    chunkVb.attributes = chunkAttrs.data();
    chunkVb.arrayStride = 0;
    chunkVb.stepMode = WGPUVertexStepMode_Instance;

    const std::array vbLayouts{ voxelVb, chunkVb };

    WGPUFragmentState frag{};
    frag.module = shaderModule;
    frag.entryPoint = WGPUStringView{"fsShadow", WGPU_STRLEN};
    frag.targetCount = 0;
    frag.targets = nullptr;

    WGPURenderPipelineDescriptor rp{};
    rp.layout = pipelineLayout;
    rp.vertex.module = shaderModule;
    rp.vertex.entryPoint = WGPUStringView{"vsShadow", WGPU_STRLEN};
    rp.vertex.bufferCount = vbLayouts.size();
    rp.vertex.buffers = vbLayouts.data();
    const std::array vConsts{ WGPUConstantEntry{ .key = WGPUStringView{"CHUNK_SIZE", WGPU_STRLEN}, .value = static_cast<double>(chunkSize) } };
    rp.vertex.constantCount = vConsts.size();
    rp.vertex.constants = vConsts.data();
    rp.fragment = &frag;
    rp.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rp.primitive.frontFace = WGPUFrontFace_CCW;
    rp.primitive.cullMode = WGPUCullMode_None;

    WGPUDepthStencilState ds{};
    ds.format = WGPUTextureFormat_Depth32Float;
    ds.depthWriteEnabled = WGPUOptionalBool_True;
    ds.depthCompare = WGPUCompareFunction_Less;
    rp.depthStencil = &ds;
    rp.multisample.count = 1;
    rp.multisample.mask = ~0u;

    out.pipeline = wgpuDeviceCreateRenderPipeline(device, &rp);

    chunkVb.arrayStride = sizeof(glm::vec4);
    const std::array remotePlayerVbLayouts{ voxelVb, chunkVb };
    rp.vertex.buffers = remotePlayerVbLayouts.data();
    out.remotePlayerPipeline = wgpuDeviceCreateRenderPipeline(device, &rp);

    // Release intermediates
    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupLayoutRelease(bgl);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return out;
}
