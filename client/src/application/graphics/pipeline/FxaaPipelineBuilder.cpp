#include "FxaaPipelineBuilder.h"
#include "common/FileSystem.h"

WGPURenderPipeline FxaaPipelineBuilder::build(WGPUBindGroupLayout bindGroupLayout, WGPUTextureFormat surfaceFormat) const {
    // Load FXAA shader from file
    auto shaderBuf = FileSystem::readFileNative("shaders/fxaa.wgsl");
    std::string shaderCode(shaderBuf.begin(), shaderBuf.end());

    WGPUShaderSourceWGSL shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderCodeDesc.code = WGPUStringView{shaderCode.c_str(), shaderCode.size()};

    WGPUShaderModuleDescriptor shaderDesc{};
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    shaderDesc.label = WGPUStringView{"FXAA Shader", WGPU_STRLEN};
    WGPUShaderModule fxaaShaderModule = wgpuDeviceCreateShaderModule(m_device, &shaderDesc);

    // Create pipeline layout using the FXAA bind group layout
    WGPUPipelineLayoutDescriptor layoutDesc = {};
    layoutDesc.nextInChain = nullptr;
    layoutDesc.bindGroupLayoutCount = 1;
    layoutDesc.bindGroupLayouts = &bindGroupLayout;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_device, &layoutDesc);

    WGPUVertexState vertexState = {};
    vertexState.module = fxaaShaderModule;
    vertexState.entryPoint = WGPUStringView{"vs_main", WGPU_STRLEN};
    vertexState.bufferCount = 0;
    vertexState.buffers = nullptr;

    // Fragment state
    WGPUFragmentState fragmentState = {};
    fragmentState.module = fxaaShaderModule;
    fragmentState.entryPoint = WGPUStringView{"fs_main", WGPU_STRLEN};
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = surfaceFormat;
    colorTarget.writeMask = WGPUColorWriteMask_All;
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    // Pipeline descriptor
    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.vertex = vertexState;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipelineDesc);
    wgpuShaderModuleRelease(fxaaShaderModule);
    wgpuPipelineLayoutRelease(pipelineLayout);
    return pipeline;
}
