// Vulkan 3DGS - Copyright (c) 2025 Alejandro Amat (github.com/AlejandroAmat) -
// MIT Licensed

#include "GaussianRenderer.h"

GaussianRenderer::~GaussianRenderer() {
  _bufferManager.CleanupAllBuffers(_vulkanContext.GetLogicalDevice());
  std::cout << "GaussianRenderer destroyed" << std::endl;
}

void GaussianRenderer::LoadGaussianData(
    std::unique_ptr<GaussianBase> gaussianData) {
  if (!gaussianData) {
    throw std::runtime_error("Cannot load null gaussian data!");
  }

  std::cout << "\n=== GaussianRenderer: Loading Data ===" << std::endl;
  std::cout << "Gaussians: " << gaussianData->GetCount() << std::endl;
  std::cout << "SH degree: " << gaussianData->GetSHDegree() << std::endl;

  _gaussianData = std::move(gaussianData);

  _nGauss = uint32_t(_gaussianData->GetCount());
  std::cout << " Gaussian data loaded!" << std::endl;
}

void GaussianRenderer::InitComputePipeline() {
  _imguiHandler.Init();
  _graphcsPipeline.setBufferManager(&_bufferManager);
  _graphcsPipeline.Init();
  _computePipeline.setNumGaussians(_nGauss);
  g_renderSettings.numGaussians = _nGauss;
  _computePipeline.Initialize(_buffers);
}

void GaussianRenderer::CreateBuffers() {
  CreateGaussianBuffers();
  CreateUniformBuffer();
  CreatePipelineStorageBuffers();
  CreateCopyStagingBuffer();
  _computePipeline.setBufferManager(&_bufferManager);
}

void GaussianRenderer::Render() { _computePipeline.RenderFrame(*_camera); }

void GaussianRenderer::InitializeCamera(float windowWidth, float windowHeight) {
  float aspectRatio = windowWidth / windowHeight;
  _camera = std::make_shared<Camera>(int(windowWidth), int(windowHeight), 45.0f,
                                     aspectRatio, 0.1f, 1000.0f);

  _camera->SetMovementSpeed(3.0f);
  _camera->SetMouseSensitivity(0.04f);

  g_renderSettings.mouseSensitivity = 0.04f;
  g_renderSettings.speed = 3.0f;
  g_renderSettings.fov = 45.0f;
  g_renderSettings.nearPlane = 0.1f;
  g_renderSettings.farPlane = 45.0f;
  g_renderSettings.enableCulling = true;

  glfwSetWindowUserPointer(_vulkanContext.getWindow(), this);
  // Set up mouse callback
  glfwSetCursorPosCallback(_vulkanContext.getWindow(), mouse_callback);

  // Capture mouse cursor
  glfwSetInputMode(_vulkanContext.getWindow(), GLFW_CURSOR,
                   GLFW_CURSOR_DISABLED);
  std::cout << "Camera initialized with aspect ratio: " << aspectRatio
            << std::endl;
}

void GaussianRenderer::mouse_callback(GLFWwindow *window, double xpos,
                                      double ypos) {
  GaussianRenderer *renderer =
      static_cast<GaussianRenderer *>(glfwGetWindowUserPointer(window));
  if (renderer && renderer->_camera) {
    static bool firstMouse = true;
    static float lastX, lastY;

    bool rightMousePressed =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (firstMouse) {
      lastX = static_cast<float>(xpos);
      lastY = static_cast<float>(ypos);
      firstMouse = false;
      return;
    }

    float deltaX = static_cast<float>(xpos) - lastX;
    float deltaY = lastY - static_cast<float>(ypos);

    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);
    if (!rightMousePressed) {
      firstMouse = false;
      g_renderSettings.camRotationActive = false;
      return;
    }
    g_renderSettings.camRotationActive = true;
    renderer->_camera->ProcessMouseMovement(deltaX, deltaY);
  }
}

void GaussianRenderer::processInput(float deltaTime) {
  GLFWwindow *window = _vulkanContext.getWindow();
  if (g_renderSettings.playing) {
    _camera->ProcessKeyboard(CameraMovement::SEQ, deltaTime);
  }
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::FORWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::BACKWARD, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::LEFT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::RIGHT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::UP, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::DOWN, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::ROLL_RIGHT, deltaTime);
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    _camera->ProcessKeyboard(CameraMovement::ROLL_LEFT, deltaTime);

  // ESC to exit
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}

void GaussianRenderer::CreateGaussianBuffers() {
  std::cout << "\n--- Creating Gaussian GPU Buffers and uploading to GPU "
               "through staging buffers ---"
            << std::endl;

  CreateAndUploadBuffer<glm::vec4>(_buffers.xyz,
                                   _gaussianData->GetPositionsData(), "_xyz");
  CreateAndUploadBuffer<glm::vec4>(_buffers.scales,
                                   _gaussianData->GetScalesData(), "_scale");
  CreateAndUploadBuffer<glm::vec4>(_buffers.rotations,
                                   _gaussianData->GetRotationsData(), "_rot");
  CreateAndUploadBuffer<float>(_buffers.opacity,
                               _gaussianData->GetOpacitiesData(), "_opacity");
  CreateAndUploadBuffer<float>(
      _buffers.sh, _gaussianData->GetSHData(), "_SH",
      3 * (_gaussianData->GetSHCoefficientsPerChannel()));
}

void GaussianRenderer::CreatePipelineStorageBuffers() {
  CreateWriteBuffers<int>(_buffers.radii, "radii", 1);
  CreateWriteBuffers<float>(_buffers.depth, "depth", 1);
  CreateWriteBuffers<glm::vec4>(_buffers.color, "color");
  CreateWriteBuffers<glm::vec4>(_buffers.conicOpacity, "conicOpacity");
  CreateWriteBuffers<glm::vec2>(_buffers.points2d, "points2d");
  CreateWriteBuffers<int>(_buffers.tilesTouched, "tilesTouched", 1, true);
  CreateWriteBuffers<int>(_buffers.tilesTouchedPrefixSum,
                          "tilesTouchedPrefixSum", 1, true);
  CreateWriteBuffers<glm::vec4>(_buffers.boundingBox, "boundingBox");
  CreateWriteBuffers<uint64_t>(_buffers.keysRadix, "keysRadix", 10, true);
  CreateWriteBuffers<uint64_t>(_buffers.keys, "keys", 10, true);
  CreateWriteBuffers<uint32_t>(_buffers.values, "values", 10, true);
  CreateWriteBuffers<uint32_t>(_buffers.valuesRadix, "valuesRadix", 10, true);
  CreateWriteBuffers<uint32_t>(_buffers.histogram, "histogram", 10, true);
  CreateRangesBuffer();
}
void GaussianRenderer::UpdateCameraUniforms() {
  CameraUniforms uniforms = _camera->getUniforms();
  uniforms.shDegree = _shDegree;

  CameraUniforms *gpuData = (CameraUniforms *)_cameraUniformMapped;

  // Write to GPU
  memcpy(_cameraUniformMapped, &uniforms, sizeof(uniforms));
}

void GaussianRenderer::CreateUniformBuffer() {

  VkDevice device = _vulkanContext.GetLogicalDevice();
  VkPhysicalDevice physicalDevice = _vulkanContext.GetPhysicalDevice();

  CameraUniforms camUniforms = _camera->getUniforms();
  camUniforms.shDegree = 3;
  VkDeviceSize bufferSize = sizeof(camUniforms);
  std::cout << " Creating uniform buffer : " << bufferSize << " bytes "
            << std::endl;

  _buffers.camUniform =
      _bufferManager.CreateUniformBuffer(device, physicalDevice, bufferSize);

  VkDeviceMemory mem = _bufferManager.GetBufferMemory(_buffers.camUniform);
  vkMapMemory(device, mem, 0, bufferSize, 0, &_cameraUniformMapped);

  UpdateCameraUniforms();
}

void GaussianRenderer::CreateCopyStagingBuffer() {

  VkDevice device = _vulkanContext.GetLogicalDevice();
  VkPhysicalDevice physicalDevice = _vulkanContext.GetPhysicalDevice();

  VkDeviceSize bufferSize = sizeof(int) * 100;

  _buffers.numRendered.staging = _bufferManager.CreateBuffer(
      device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  vkMapMemory(device,
              _bufferManager.GetBufferMemory(_buffers.numRendered.staging), 0,
              bufferSize, 0, &_buffers.numRendered.mem);
}

void GaussianRenderer::CreateRangesBuffer() {
  VkExtent2D ext = _vulkanContext.GetSwapchainExtent();
  int windowResize = 1;
#ifdef __APPLE__
  windowResize = 2;
#endif
  int tiles = (ext.width / windowResize + 15) / 16 *
              (ext.height / windowResize + 15) / 16;

  _buffers.ranges = _bufferManager.CreateBuffer(
      _vulkanContext.GetLogicalDevice(), _vulkanContext.GetPhysicalDevice(),
      sizeof(glm::vec2) * tiles,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
