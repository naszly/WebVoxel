#include "PipelineBuilder.h"

#include <webgpu/webgpu.h>

#include "application/graphics/BlockTextureManager.h"
#include "application/types/VertexData.h"
#include "application/world/chunk/Chunk.h"
#include "common/FileSystem.h"

PipelineArtifacts PipelineBuilder::build(const PipelineOptions& options,
                                         const WGPUBuffer& uniformBuffer,
                                         const BlockTextureManager& blockTextures) const {
    PipelineArtifacts out{};

    // Load shader
    WGPUShaderModuleDescriptor shaderDesc{};
    auto shaderBuf = FileSystem::readFileNative("shaders/shader.wgsl");
    std::string shaderCode(shaderBuf.begin(), shaderBuf.end());

    WGPUShaderSourceWGSL shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    shaderCodeDesc.code = WGPUStringView{shaderCode.c_str(), shaderCode.size()};
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(m_device, &shaderDesc);

    // Bind group layout entries
    WGPUBindGroupLayoutEntry uniformsBgl{};
    uniformsBgl.binding = 0;
    uniformsBgl.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    uniformsBgl.buffer.type = WGPUBufferBindingType_Uniform;
    uniformsBgl.buffer.hasDynamicOffset = false;
    uniformsBgl.buffer.minBindingSize = wgpuBufferGetSize(uniformBuffer);

    WGPUBindGroupLayoutEntry textureIdsBgl{};
    textureIdsBgl.binding = 1;
    textureIdsBgl.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    textureIdsBgl.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    textureIdsBgl.buffer.hasDynamicOffset = false;
    textureIdsBgl.buffer.minBindingSize = 0;

    WGPUBindGroupLayoutEntry textureArrayBgl{};
    textureArrayBgl.binding = 2;
    textureArrayBgl.visibility = WGPUShaderStage_Fragment;
    textureArrayBgl.texture.sampleType = WGPUTextureSampleType_Float;
    textureArrayBgl.texture.viewDimension = WGPUTextureViewDimension_2DArray;
    textureArrayBgl.texture.multisampled = false;

    std::array bglEntries{
        uniformsBgl,
        textureIdsBgl,
        textureArrayBgl
    };
    WGPUBindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = bglEntries.size();
    bglDesc.entries = bglEntries.data();
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(m_device, &bglDesc);

    // Bind group entries
    WGPUBindGroupEntry uboEntry{};
    uboEntry.binding = 0;
    uboEntry.buffer = uniformBuffer;
    uboEntry.offset = 0;
    uboEntry.size = wgpuBufferGetSize(uniformBuffer);

    WGPUBindGroupEntry textureIdsEntry{};
    textureIdsEntry.binding = 1;
    textureIdsEntry.buffer = blockTextures.getTextureIdsBuffer();
    textureIdsEntry.offset = 0;
    textureIdsEntry.size = blockTextures.getTextureIdsBufferSize();

    WGPUBindGroupEntry textureArrayEntry{};
    textureArrayEntry.binding = 2;
    textureArrayEntry.sampler = nullptr;
    textureArrayEntry.textureView = blockTextures.getTextureArrayView();

    std::array bgEntries{
        uboEntry,
        textureIdsEntry,
        textureArrayEntry
    };
    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = bgEntries.size();
    bgDesc.entries = bgEntries.data();
    out.uniformBindGroup = wgpuDeviceCreateBindGroup(m_device, &bgDesc);

    // Pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &pipelineLayoutDesc);

    // Vertex buffer layouts
    WGPUVertexBufferLayout billboardVb{};
    WGPUVertexAttribute billboardAttr{};
    billboardAttr.shaderLocation = 0;
    billboardAttr.format = WGPUVertexFormat_Float32x2;
    billboardAttr.offset = 0;
    billboardVb.attributeCount = 1;
    billboardVb.attributes = &billboardAttr;
    billboardVb.arrayStride = 2 * sizeof(float);
    billboardVb.stepMode = WGPUVertexStepMode_Vertex;

    WGPUVertexBufferLayout voxelVb{};
    constexpr std::array voxelAttrs{
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = 0, .shaderLocation = 1 },
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = offsetof(VertexData, voxel), .shaderLocation = 2 },
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = offsetof(VertexData, ambientOcclusion), .shaderLocation = 4 },
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Uint32, .offset = offsetof(VertexData, pointLight), .shaderLocation = 5 },
    };
    voxelVb.attributeCount = voxelAttrs.size();
    voxelVb.attributes = voxelAttrs.data();
    voxelVb.arrayStride = sizeof(VertexData);
    voxelVb.stepMode = WGPUVertexStepMode_Instance;

    WGPUVertexBufferLayout chunkVb{};
    constexpr std::array chunkAttrs{
        WGPUVertexAttribute{ .format = WGPUVertexFormat_Float32x4, .offset = 0, .shaderLocation = 3 }
    };
    chunkVb.attributeCount = chunkAttrs.size();
    chunkVb.attributes = chunkAttrs.data();
    chunkVb.arrayStride = 0;
    chunkVb.stepMode = WGPUVertexStepMode_Instance;

    const std::array vbLayouts{ billboardVb, voxelVb, chunkVb };

    // Pipeline descriptor
    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.layout = pipelineLayout;

    const std::array vertexConstants{
        WGPUConstantEntry{ .key = WGPUStringView{"CHUNK_SIZE", WGPU_STRLEN}, .value = options.chunkSize() }
    };

    pipelineDesc.vertex.bufferCount = vbLayouts.size();
    pipelineDesc.vertex.buffers = vbLayouts.data();
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = WGPUStringView{"vsMain", WGPU_STRLEN};
    pipelineDesc.vertex.constantCount = vertexConstants.size();
    pipelineDesc.vertex.constants = vertexConstants.data();

    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

    WGPUBlendState blend{};
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.color.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = WGPUBlendFactor_Zero;
    blend.alpha.dstFactor = WGPUBlendFactor_One;
    blend.alpha.operation = WGPUBlendOperation_Add;

    WGPUColorTargetState colorTarget{};
    colorTarget.format = options.colorFormat();
    colorTarget.blend = &blend;
    colorTarget.writeMask = WGPUColorWriteMask_All;

    const std::array fragmentConstants{
        WGPUConstantEntry{ .key = WGPUStringView{"LIGHTING", WGPU_STRLEN}, .value = options.lighting() },
        WGPUConstantEntry{ .key = WGPUStringView{"FOG", WGPU_STRLEN}, .value = options.fog() },
        WGPUConstantEntry{ .key = WGPUStringView{"POINT_LIGHT", WGPU_STRLEN}, .value = options.pointLight() },
    };

    WGPUFragmentState frag{};
    frag.module = shaderModule;
    frag.entryPoint = WGPUStringView{"fsMain", WGPU_STRLEN};
    frag.constantCount = fragmentConstants.size();
    frag.constants = fragmentConstants.data();
    frag.targetCount = 1;
    frag.targets = &colorTarget;

    pipelineDesc.fragment = &frag;

    WGPUDepthStencilState ds{};
    ds.format = WGPUTextureFormat_Depth24PlusStencil8;
    ds.depthWriteEnabled = WGPUOptionalBool_True;
    ds.depthCompare = WGPUCompareFunction_Less;
    ds.stencilFront.compare = WGPUCompareFunction_Always;
    ds.stencilFront.failOp = WGPUStencilOperation_Keep;
    ds.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    ds.stencilFront.passOp = WGPUStencilOperation_Keep;
    ds.stencilBack = ds.stencilFront;

    pipelineDesc.depthStencil = &ds;
    pipelineDesc.multisample.count = options.sampleCount();
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    out.pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);

    // Cleanup locals
    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pipelineLayout);

    return out;
}
