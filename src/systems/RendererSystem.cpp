#include "RendererSystem.h"

#include <fstream>

#include "../Application.h"
#include "../ApplicationEvent.h"
#include "../Log.h"

void RendererSystem::initialize() {
    LogApp::info("RendererSystem::initialize");

    m_Queue = wgpuDeviceGetQueue(GetWebGPUContext().getDevice());

	m_viewportWidth = GetWebGPUSurface().getWidth();
	m_viewportHeight = GetWebGPUSurface().getHeight();

	Camera& camera = Application::GetInstance().getCamera();
	constexpr float fov = glm::radians(66.0);
	camera.setPerspective(fov, (float)m_viewportWidth / (float)m_viewportHeight);

	InitializeBuffers();
	createDepthTexture();
    createRenderPipeline();
}

void RendererSystem::render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) {
    const Camera& camera = Application::GetInstance().getCamera();

	uniformData.projectionViewMatrix = camera.getProjectionViewMatrix();
	uniformData.inverseProjectionViewMatrix = camera.getInverseProjectionViewMatrix();
	uniformData.cameraPosition = camera.getPosition();
	uniformData.fov = glm::radians(66.0);
	uniformData.viewportSize = {m_viewportWidth, m_viewportHeight};
	uniformData.nearPlane = Camera::NEAR;
	uniformData.farPlane = Camera::FAR;

	// Update uniform buffer data
	wgpuQueueWriteBuffer(m_Queue, uniformBuffer, 0, &uniformData, sizeof(Uniforms));

	// Create the render pass that clears the screen with our color
	WGPURenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;

	// The attachment part of the render pass descriptor describes the target texture of the pass
	WGPURenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	WGPURenderPassDepthStencilAttachment depthStencilAttachment = {};
	depthStencilAttachment.view = depthTextureView;
	depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
	depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
	depthStencilAttachment.depthClearValue = 1.0f;
	depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
	depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
	depthStencilAttachment.stencilClearValue = 0;

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
	renderPassDesc.timestampWrites = nullptr;

    Application& app = Application::GetInstance();
	auto& world = app.getWorld();

	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

	// Select which render pipeline to use
	wgpuRenderPassEncoderSetPipeline(renderPass, m_RenderPipeline);

	// Set the bind group
	wgpuRenderPassEncoderSetBindGroup(renderPass, 0, uniformBindGroup, 0, nullptr);

	// Set vertex buffer while encoding the render pass
	wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, wgpuBufferGetSize(vertexBuffer));

	// Set index buffer
	wgpuRenderPassEncoderSetIndexBuffer(renderPass, indexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize(indexBuffer));

	for (auto&[buffer, vertexCount] : world.getChunkVertexBuffers()) {

		if (vertexCount == 0) {
			continue;
		}

		// Update uniform buffer data
		wgpuQueueWriteBuffer(m_Queue, uniformBuffer, 0, &uniformData, sizeof(Uniforms));

		// Set voxel buffer
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 1, buffer, 0, wgpuBufferGetSize(buffer) - sizeof(glm::vec4));

		// Set chunk buffer
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 2, buffer, wgpuBufferGetSize(buffer) - sizeof(glm::vec4), sizeof(glm::vec4));

		// Use instanced drawing
		wgpuRenderPassEncoderDrawIndexed(renderPass, indexCount, vertexCount, 0, 0, 0);
	}
	wgpuRenderPassEncoderEnd(renderPass);
	wgpuRenderPassEncoderRelease(renderPass);

}

void RendererSystem::update(float dt) {
}

void RendererSystem::onEvent(Event& event) {
	EventDispatcher dispatcher(event);

	dispatcher.dispatch<WindowResizedEvent>([&](const WindowResizedEvent &windowResizedEvent) {
		LogApp::info("WindowResizedEvent: {0}, {1}", windowResizedEvent.getWidth(), windowResizedEvent.getHeight());

		m_viewportWidth = windowResizedEvent.getWidth();
		m_viewportHeight = windowResizedEvent.getHeight();

		createDepthTexture();

		Camera& camera = Application::GetInstance().getCamera();
		constexpr float fov = glm::radians(66.0);
		camera.setPerspective(fov, (float)m_viewportWidth / (float)m_viewportHeight);

		return true;
	});
}

void RendererSystem::createRenderPipeline() {
	const auto device = GetWebGPUContext().getDevice();
    const auto surfaceFormat = GetWebGPUSurface().getSurfaceFormat();

    // Load the shader module
    WGPUShaderModuleDescriptor shaderDesc{};
    std::string shaderCode = LoadShader("shaders/shader.wgsl");

    WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
    shaderCodeDesc.chain.next = nullptr;
#ifdef WEBGPU_BACKEND_DAWN
    shaderCodeDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
#else
	shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
#endif
    shaderDesc.nextInChain = &shaderCodeDesc.chain;
    shaderCodeDesc.code = shaderCode.c_str();
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    // Create the bind group layout
    WGPUBindGroupLayoutEntry bglEntry{};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bglEntry.buffer.type = WGPUBufferBindingType_Uniform;
    bglEntry.buffer.hasDynamicOffset = false;
    bglEntry.buffer.minBindingSize = sizeof(Uniforms);

    WGPUBindGroupLayoutDescriptor bglDesc{};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    // Create the bind group
    WGPUBindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = uniformBuffer;
    bgEntry.offset = 0;
    bgEntry.size = sizeof(Uniforms);

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout = bindGroupLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    uniformBindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    // Create the pipeline layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc{};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &bindGroupLayout;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // Create the render pipeline
    WGPURenderPipelineDescriptor pipelineDesc{};
    pipelineDesc.nextInChain = nullptr;
    pipelineDesc.layout = pipelineLayout;

    // Configure the vertex pipeline
    WGPUVertexBufferLayout billboardVertexBufferLayout{};
    WGPUVertexAttribute billboardAttributes{};
    billboardAttributes.shaderLocation = 0;
    billboardAttributes.format = WGPUVertexFormat_Float32x2;
    billboardAttributes.offset = 0;

    billboardVertexBufferLayout.attributeCount = 1;
    billboardVertexBufferLayout.attributes = &billboardAttributes;
    billboardVertexBufferLayout.arrayStride = 2 * sizeof(float);
    billboardVertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

    // Configure the instance buffer layout
    WGPUVertexBufferLayout voxelVertexBufferLayout{};
    std::vector<WGPUVertexAttribute> voxelAttributes{};

	// Position
	voxelAttributes.push_back({
		.format = WGPUVertexFormat_Uint32,
		.offset = 0,
		.shaderLocation = 1,
	});

	// Color
	voxelAttributes.push_back({
		.format = WGPUVertexFormat_Uint32,
		.offset = 4 * sizeof(uint8_t),
		.shaderLocation = 2,
	});

    voxelVertexBufferLayout.attributeCount = voxelAttributes.size();
    voxelVertexBufferLayout.attributes = voxelAttributes.data();
    voxelVertexBufferLayout.arrayStride = 8 * sizeof(uint8_t);
    voxelVertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;

	WGPUVertexBufferLayout chunkVertexBufferLayout{};
	std::vector<WGPUVertexAttribute> chunkAttributes{};

	chunkAttributes.push_back({
		.format = WGPUVertexFormat_Float32x4,
		.offset = 0,
		.shaderLocation = 3,
	});

	chunkVertexBufferLayout.attributeCount = chunkAttributes.size();
	chunkVertexBufferLayout.attributes = chunkAttributes.data();
	chunkVertexBufferLayout.arrayStride = 0;
	chunkVertexBufferLayout.stepMode = WGPUVertexStepMode_Instance;

    WGPUVertexBufferLayout bufferLayouts[] = { billboardVertexBufferLayout, voxelVertexBufferLayout, chunkVertexBufferLayout };

	WGPUConstantEntry pipelineConstant[] {
		{
			.key = "CHUNK_SIZE",
			.value = Chunk::SIZE,
		}
	};

    pipelineDesc.vertex.bufferCount = sizeof(bufferLayouts) / sizeof(WGPUVertexBufferLayout);
    pipelineDesc.vertex.buffers = bufferLayouts;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = sizeof(pipelineConstant) / sizeof(WGPUConstantEntry);
	pipelineDesc.vertex.constants = pipelineConstant;

    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;

	WGPUBlendState blendState{};
	blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
	blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
	blendState.color.operation = WGPUBlendOperation_Add;
	blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
	blendState.alpha.dstFactor = WGPUBlendFactor_One;
	blendState.alpha.operation = WGPUBlendOperation_Add;

	WGPUColorTargetState colorTarget{};
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragmentState{};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

	// Configure the depth-stencil state
	WGPUDepthStencilState depthStencilState{};
	depthStencilState.format = WGPUTextureFormat_Depth24PlusStencil8;
#ifdef WEBGPU_BACKEND_DAWN
	depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
#else
	depthStencilState.depthWriteEnabled = true;
#endif
	depthStencilState.depthCompare = WGPUCompareFunction_Less;
	depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
	depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
	depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
	depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
	depthStencilState.stencilBack = depthStencilState.stencilFront;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = &depthStencilState;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    m_RenderPipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);

    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pipelineLayout);
}

void RendererSystem::createDepthTexture() {
	const auto device = GetWebGPUContext().getDevice();

	if (depthTexture) {
		wgpuTextureRelease(depthTexture);
		wgpuTextureViewRelease(depthTextureView);
	}

	WGPUTextureDescriptor textureDesc = {};
	textureDesc.size.width = m_viewportWidth;
	textureDesc.size.height = m_viewportHeight;
	textureDesc.size.depthOrArrayLayers = 1;
	textureDesc.mipLevelCount = 1;
	textureDesc.sampleCount = 1;
	textureDesc.dimension = WGPUTextureDimension_2D;
	textureDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
	textureDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;

	depthTexture = wgpuDeviceCreateTexture(device, &textureDesc);
	if (!depthTexture) {
		LogCore::error("Failed to create depth texture");
		return;
	}

	WGPUTextureViewDescriptor viewDesc = {};
	viewDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
	viewDesc.dimension = WGPUTextureViewDimension_2D;
	viewDesc.baseMipLevel = 0;
	viewDesc.mipLevelCount = 1;
	viewDesc.baseArrayLayer = 0;
	viewDesc.arrayLayerCount = 1;
	viewDesc.aspect = WGPUTextureAspect_All;

	depthTextureView = wgpuTextureCreateView(depthTexture, &viewDesc);
	if (!depthTextureView) {
		LogCore::error("Failed to create depth texture view");
		wgpuTextureRelease(depthTexture);
	}
}

std::string RendererSystem::LoadShader(const char* filename) {
	std::ifstream file(filename, std::ios::binary);

	if (!file) {
		LogWebGPU::error("Failed to open file: {0}", filename);
		return "";
	}

	std::string shaderCode((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	return shaderCode;
}

void RendererSystem::InitializeBuffers() {
	const auto device = GetWebGPUContext().getDevice();

	// Vertex buffer data
	std::vector<float> vertexData = {
		-1.0f, -1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f,
		-1.0f, 1.0f,
	};
	vertexCount = static_cast<uint32_t>(vertexData.size() / 2);

	// Create vertex buffer
	WGPUBufferDescriptor bufferDesc{};
	bufferDesc.nextInChain = nullptr;
	bufferDesc.size = vertexData.size() * sizeof(float);
	bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex; // Vertex usage here!
	bufferDesc.mappedAtCreation = false;
	vertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

	// Upload geometry data to the buffer
	wgpuQueueWriteBuffer(m_Queue, vertexBuffer, 0, vertexData.data(), bufferDesc.size);

	// Index buffer data
	std::vector<uint16_t> indexData = {
		0, 1, 2,
		0, 2, 3,
	};
	indexCount = static_cast<uint32_t>(indexData.size());

	// Create index buffer
	WGPUBufferDescriptor indexBufferDesc{};
	indexBufferDesc.nextInChain = nullptr;
	indexBufferDesc.size = indexData.size() * sizeof(uint16_t);
	indexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
	indexBufferDesc.mappedAtCreation = false;
	indexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);

	// Upload index data to the buffer
	wgpuQueueWriteBuffer(m_Queue, indexBuffer, 0, indexData.data(), indexBufferDesc.size);

	// Create uniform buffer
	WGPUBufferDescriptor uniformBufferDesc{};
	uniformBufferDesc.nextInChain = nullptr;
	uniformBufferDesc.size = sizeof(Uniforms);
	uniformBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
	uniformBufferDesc.mappedAtCreation = false;
	uniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);
}