// Vulkan 3DGS - Copyright (c) 2025 Alejandro Amat (github.com/AlejandroAmat) -
// MIT Licensed

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <vector>

struct StagingRead {
  VkBuffer staging;
  void *mem;
};

struct FrameTimer {
  std::chrono::steady_clock::time_point lastTime;
  double fps;
  double deltaTime;

  FrameTimer()
      : lastTime(std::chrono::steady_clock::now()), fps(0.0), deltaTime(0.0) {}

  void UpdateTime() {
    auto currentTime = std::chrono::steady_clock::now();
    deltaTime = std::chrono::duration<double>(currentTime - lastTime).count();
    lastTime = currentTime;

    if (deltaTime > 0.0) {
      fps = 1.0 / deltaTime;
    }
  }
  void PrintStats() const {
    std::cout << "FPS: " << fps << " | Frame Time: " << deltaTime * 1000.0
              << "ms" << std::endl;
  }
};

struct InputArgs {
  std::string ply;
  int degree;
};
constexpr int AVG_GAUSS_TILE = 4;

struct GaussianBuffers {
  VkBuffer xyz;
  VkBuffer scales;
  VkBuffer rotations;
  VkBuffer opacity;
  VkBuffer sh;
  VkBuffer camUniform;
  VkBuffer radii;
  VkBuffer depth;
  VkBuffer color;
  VkBuffer conicOpacity;
  VkBuffer points2d;
  VkBuffer tilesTouched;
  VkBuffer tilesTouchedPrefixSum;
  VkBuffer boundingBox;
  StagingRead numRendered;
  VkBuffer keys;
  VkBuffer valuesRadix;
  VkBuffer keysRadix;
  VkBuffer values;
  VkBuffer ranges;
  VkBuffer histogram;
};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

struct QueueFamilyIndices {
  int graphicsFamily = -1; // location
  int presentationFamily = -1;
  bool isValid() { return graphicsFamily >= 0 && presentationFamily >= 0; }
};

struct SwapChainDetails {
  VkSurfaceCapabilitiesKHR surfaceCapabilities;    // surface properties
  std::vector<VkSurfaceFormatKHR> imageFormat;     // RGB, HSV...
  std::vector<VkPresentModeKHR> presentationsMode; // presentationMode
};

struct SwapChainImage {
  VkImage image;
  VkImageView imageView;
};

static std::vector<char> ReadFile(const std::string &filename) {
  std::ifstream file(filename, std::ios::binary | std::ios::ate);

  if (!file.is_open()) {
    throw std::runtime_error("Failed opening file!");
  }
  size_t fileSize = (size_t)file.tellg();
  std::vector<char> fileBuffer(fileSize);
  file.seekg(0);

  file.read(fileBuffer.data(), fileSize);

  file.close();

  return fileBuffer;
}

static InputArgs checkArgs(int argc, char *argv[]) {
  if (argc < 3) {
    std::cout << "here" << std::endl;
    throw std::invalid_argument("Usage: " + std::string(argv[0]) +
                                " <poiv ntcloud_file> <SH_degree>");
  }

  std::string pointcloudPath = argv[1];
  int shDegree = std::atoi(argv[2]);

  if (!std::filesystem::exists(pointcloudPath)) {
    throw std::runtime_error("File '" + pointcloudPath + "' does not exist!");
  }

  if (shDegree < 0 || shDegree > 3) {
    throw std::invalid_argument("SH degree " + std::to_string(shDegree) +
                                " not supported (0-3)");
  }

  return {pointcloudPath, shDegree};
}