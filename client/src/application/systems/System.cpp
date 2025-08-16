#include "System.h"
#include "application/Application.h"

ApplicationData& System::getApplicationData() const { return m_app->getApplicationData(); }

Camera& System::getCamera() const { return m_app->getCamera(); }

const Input& System::getInput() const { return m_app->getInput(); }

World& System::getWorld() const { return m_app->getWorld(); }

const WebGpuContext& System::getWebGpuContext() const { return *m_app->getWebGpuContext(); }

const WebGpuSurface& System::getWebGpuSurface() const { return m_app->getWebGpuSurface(); }

GLFWwindow* System::getGlfwWindow() const { return m_app->getGlfwWindow(); }

BlockTextureManager& System::getBlockTextureManager() const { return m_app->getBlockTextureManager(); }
