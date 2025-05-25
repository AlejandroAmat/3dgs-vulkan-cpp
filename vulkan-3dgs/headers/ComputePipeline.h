#pragma once

#define GLFW_INCLUDE_VULKAN
#include "VulkanContext.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <map>
#include <vector>

constexpr int frames_in_flight = 2;

struct DescriptorBinding {
  uint32_t binding;
  VkDescriptorType type;
  VkShaderStageFlags stageFlags;
  uint32_t count = 1;
};

enum class PipelineType {
  DEBUG_RED_FILL,
  CULLING,
  SORTING,
  SPLATTING,
};

class ComputePipeline {
public:
  ComputePipeline(VulkanContext &vkContext) : _vkContext(vkContext){};
  ~ComputePipeline() { CleanUp(); }

  void Initialize();
  void RenderFrame();
  void CleanUp();

private:
  VulkanContext &_vkContext;

  std::vector<VkCommandBuffer> _commandBuffers;
  std::vector<VkFence> _fences;
  std::vector<VkSemaphore> _semaphores;

  std::map<PipelineType, VkDescriptorSetLayout> _descriptorSetLayouts;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;
  std::map<PipelineType, std::vector<VkDescriptorSet>> _descriptorSets;

  std::map<PipelineType, VkPipelineLayout> _pipelineLayouts;
  std::map<PipelineType, VkPipeline> _computePipelines;
  uint32_t _currentFrame = 0;

  void CreateCommandBuffers();
  void CreateSynchronization();
  void CreateDescriptorSetLayout(const PipelineType pType);
  void CreateDescriptorPool();
  void CreateComputePipeline(std::string shaderName, const PipelineType pType);
  void SetupDescriptorSet(const PipelineType pType);
  void RecordCommandBufferForImage(uint32_t imageIndex);
  VkShaderModule CreateShaderModule(const std::vector<char> &code);

  void TransitionImage(VkCommandBuffer commandBuffer, VkImageLayout in,
                       VkImageLayout out, VkImage image, VkAccessFlags src,
                       VkAccessFlags dst);
  void UpdateAllDescriptorSets(const PipelineType pType);
  void RecordAllCommandBuffers();

  // repeated layouts:: we can share them. TODO
  std::map<PipelineType, std::vector<DescriptorBinding>> SHADER_LAYOUTS = {
      {PipelineType::DEBUG_RED_FILL,
       {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1}}},

      {PipelineType::CULLING,
       {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
         1}}},

      {PipelineType::SORTING,
       {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1},
        {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
         1}}},

      {PipelineType::SPLATTING,
       {{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT,
         1}}}};
};