//
// Created by kornel on 18/11/24.
//

#include "GuiSystem.h"

#include "Application.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>

#include "ChunkManagementSystem.h"
#include "RendererSystem.h"

void GuiSystem::initialize() {

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    GLFWwindow *window = GetGLFWWindow();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplWGPU_InitInfo initInfo{};
    initInfo.Device = GetWebGPUContext().getDevice();
    initInfo.RenderTargetFormat = GetWebGPUSurface().getSurfaceFormat();
    ImGui_ImplWGPU_Init(&initInfo);

#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
#endif

    if (const auto rendererSystem = Application::GetInstance().getSystem<RendererSystem>()) {
        m_ambient_occlusion = rendererSystem->getAmbientOcclusion();
    }
}

void GuiSystem::setImGuiDisplaySize() {
    ImGuiIO& io = ImGui::GetIO();

    const int surfaceWidth = GetWebGPUSurface().getWidth();
    const int surfaceHeight = GetWebGPUSurface().getHeight();

    io.DisplaySize = ImVec2(static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight));
    io.DisplayFramebufferScale = ImVec2(1, 1);
}

void GuiSystem::render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) {

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    setImGuiDisplaySize();

    ImGui::NewFrame();

    // Draw crosshair in the center of the screen
    {
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        const int screenWidth = GetWebGPUSurface().getWidth();
        const int screenHeight = GetWebGPUSurface().getHeight();
        const float centerX = static_cast<float>(screenWidth) / 2.0f;
        const float centerY = static_cast<float>(screenHeight) / 2.0f;

        drawList->AddCircle({ centerX, centerY }, 3.0f, ImColor(255, 255, 255, 170));
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                             ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::Begin("DockSpace Demo", nullptr, windowFlags);

    ImGui::PopStyleVar(3);

    const ImGuiID dockSpaceId = ImGui::GetID("MyDockSpace");
    static ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), dockSpaceFlags);

    ImGui::End();

    ApplicationData &appData = Application::GetInstance().getApplicationData();

    ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    ImGui::Begin("Debug Info");
    static double lastTime = 0.0f;
    const double currentTime = ImGui::GetTime();
    const double frameTime = currentTime - lastTime;
    lastTime = currentTime;
    ImGui::Text("Frame Time: %.3f ms", frameTime * 1000.0f);
    ImGui::Text("Chunks: %zu", GetWorld().countChunks());
    ImGui::Text("Rendered Chunks: %zu", appData.renderedChunks);
    ImGui::Text("Rendered Voxels: %zu", appData.renderedVoxels);
    const auto position = GetCamera().getPosition();
    const auto direction = GetCamera().getDirection();
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
    ImGui::Text("Direction: (%.2f, %.2f, %.2f)", direction.x, direction.y, direction.z);

    if (ImGui::Button("Clean FS")) {
        Chunk::CleanFs();
    }

    if (const auto chunkManager = Application::GetInstance().getSystem<ChunkManagementSystem>()) {
        ImGui::SameLine();
        ImGui::BeginDisabled(chunkManager->isSaveInProgress());
        if (ImGui::Button("Save All Chunks")) {
            chunkManager->saveAllChunks();
        }
        ImGui::EndDisabled();
    }

    if (const auto renderer = Application::GetInstance().getSystem<RendererSystem>()) {
        if (ImGui::Button("Export timestamps")) {
            renderer->exportTimestamps();
        }
    }

    ImGui::Checkbox("Ambient occlusion", &m_ambient_occlusion);
    ImGui::Checkbox("Lighting", &m_lighting);
    ImGui::Checkbox("Fog", &m_fog);

    ImGui::End();

    ImGui::SetNextWindowDockID(dockSpaceId, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(0, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    ImGui::Begin("Voxel Placement");
    ImGui::ColorEdit3("Color", glm::value_ptr(appData.placedVoxelColor));
    ImGui::SliderInt("Radius", &appData.placedVoxelRadius, 0, 64);
    ImGui::Checkbox("Is Sphere", &appData.placedVoxelShapeIsSphere);
    ImGui::End();

    ImGui::Render();

    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView;
    renderPassColorAttachment.resolveTarget = nullptr;
    renderPassColorAttachment.loadOp = WGPULoadOp_Load;
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
#ifndef WEBGPU_BACKEND_WGPU
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.label = WGPUStringView{"GuiSystem RenderPass", WGPU_STRLEN};

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);
}

void GuiSystem::update(float dt) {
    if (const auto rendererSystem = Application::GetInstance().getSystem<RendererSystem>()) {
        if (rendererSystem->getAmbientOcclusion() != m_ambient_occlusion) {
            rendererSystem->setAmbientOcclusion(m_ambient_occlusion);
        }
        if (rendererSystem->getLighting() != m_lighting) {
            rendererSystem->setLighting(m_lighting);
        }
        if (rendererSystem->getFog() != m_fog) {
            rendererSystem->setFog(m_fog);
        }
    }
}

void GuiSystem::onEvent(Event &event) {
    const ImGuiIO& io = ImGui::GetIO();
    event.handled |= event.isInCategory(EventCategory::Mouse) & io.WantCaptureMouse;
    event.handled |= event.isInCategory(EventCategory::Keyboard) & io.WantCaptureKeyboard;
}
