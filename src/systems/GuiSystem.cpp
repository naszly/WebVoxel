//
// Created by kornel on 18/11/24.
//

#include "GuiSystem.h"

#include "../Application.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>

void GuiSystem::initialize() {

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

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

}

void GuiSystem::render(const WGPUCommandEncoder& encoder, const WGPUTextureView &targetView) {

    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

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

    ImGui::ShowDemoWindow();

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

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);
}

void GuiSystem::update(float dt) {
}

void GuiSystem::onEvent(Event &event) {
    const ImGuiIO& io = ImGui::GetIO();
    event.handled |= event.isInCategory(EventCategory::Mouse) & io.WantCaptureMouse;
    event.handled |= event.isInCategory(EventCategory::Keyboard) & io.WantCaptureKeyboard;
}
