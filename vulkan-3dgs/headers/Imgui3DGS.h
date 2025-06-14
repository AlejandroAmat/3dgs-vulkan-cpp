#pragma once

#include <vulkan/vulkan.h>
//
#include "Camera.h"
#include "GPUMemInfo.h"
#include "RenderSettings.h"
#include "Sequence.h"
#include "VulkanContext.h"
#include "glm/glm.hpp"
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
class ImguiUI {
public:
  ImguiUI(VulkanContext &vkContext, Sequence &seq)
      : _vkContext(vkContext), _seqRecorder(seq){};
  void Init();
  void NewFrame();
  void RecordImGuiRenderPass(VkCommandBuffer commandBuffer,
                             uint32_t imageIndex);
  void CreateUI(Camera &cam);

private:
  VulkanContext &_vkContext;
  VkDescriptorPool _descriptorPool;
  VkRenderPass _renderPass;
  std::vector<VkFramebuffer> _frameBuffers;
  std::vector<VkCommandBuffer> _commandBuffers;
  Sequence &_seqRecorder;

  void CreateRenderPass();
  void CreateFrameBuffers();
  void CreateDescriporPool();
  void CreateCommandBuffers();

  std::vector<CameraKeyframe> sequence;
  bool sequenceMode = false;
  bool playing = false;
  float currentTime = 0.0f;
  int selectedKeyframe = -1;

  int _w, _h;
  void CleanUp();
};