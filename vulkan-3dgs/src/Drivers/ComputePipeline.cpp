// Vulkan 3DGS - Copyright (c) 2025 Alejandro Amat (github.com/AlejandroAmat) -
// MIT Licensed

#include "ComputePipeline.h"

void ComputePipeline::Initialize(GaussianBuffers gaussianBuffer) {
  std::cout << "\n === Compute Pipeline Initalization === \n" << std::endl;

  _gaussianBuffers = gaussianBuffer;
  bool resultInBufferB = (_numSteps % 2) == 1;
  _resultBufferPrefix = resultInBufferB
                            ? _gaussianBuffers.tilesTouched
                            : _gaussianBuffers.tilesTouchedPrefixSum;
  CreateCommandBuffers();
  CreateDescriptorPool();

  std::string shaderPath = g_renderSettings.shaderPath;

  CreateDescriptorSetLayout(PipelineType::PREPROCESS);
  CreateComputePipeline(shaderPath + "Shaders/preprocess.spv",
                        PipelineType::PREPROCESS, 4);
  SetupDescriptorSet(PipelineType::PREPROCESS);
  UpdateAllDescriptorSets(PipelineType::PREPROCESS);

  CreateDescriptorSetLayout(PipelineType::PREFIXSUM);
  CreateComputePipeline(shaderPath + "Shaders/sum.spv", PipelineType::PREFIXSUM,
                        3);
  SetupDescriptorSet(PipelineType::PREFIXSUM);
  UpdateAllDescriptorSets(PipelineType::PREFIXSUM);

  /* CreateDescriptorSetLayout(PipelineType::NEAREST);
   CreateComputePipeline("src/Shaders/nearest.spv", PipelineType::NEAREST);
   SetupDescriptorSet(PipelineType::NEAREST);
   UpdateAllDescriptorSets(PipelineType::NEAREST);*/

  CreateDescriptorSetLayout(PipelineType::ASSIGN_TILE_IDS);
  CreateComputePipeline(shaderPath + "Shaders/idkeys.spv",
                        PipelineType::ASSIGN_TILE_IDS, 3);
  SetupDescriptorSet(PipelineType::ASSIGN_TILE_IDS);
  UpdateAllDescriptorSets(PipelineType::ASSIGN_TILE_IDS);

  CreateDescriptorSetLayout(PipelineType::RADIX_HISTOGRAM_0);
  CreateComputePipeline(shaderPath + "Shaders/histogram.spv",
                        PipelineType::RADIX_HISTOGRAM_0, 4);
  CreateDescriptorSetLayout(PipelineType::RADIX_SCATTER_0);
  CreateComputePipeline(shaderPath + "Shaders/sort.spv",
                        PipelineType::RADIX_SCATTER_0, 4);

  SetupDescriptorSet(PipelineType::RADIX_HISTOGRAM_0);
  UpdateAllDescriptorSets(PipelineType::RADIX_HISTOGRAM_0);

  SetupDescriptorSet(PipelineType::RADIX_HISTOGRAM_1);
  UpdateAllDescriptorSets(PipelineType::RADIX_HISTOGRAM_1);

  SetupDescriptorSet(PipelineType::RADIX_SCATTER_0);
  UpdateAllDescriptorSets(PipelineType::RADIX_SCATTER_0);

  SetupDescriptorSet(PipelineType::RADIX_SCATTER_1);
  UpdateAllDescriptorSets(PipelineType::RADIX_SCATTER_1);

  CreateDescriptorSetLayout(PipelineType::TILE_BOUNDARIES);
  CreateComputePipeline(shaderPath + "Shaders/boundaries.spv",
                        PipelineType::TILE_BOUNDARIES, 1);
  SetupDescriptorSet(PipelineType::TILE_BOUNDARIES);
  UpdateAllDescriptorSets(PipelineType::TILE_BOUNDARIES);

  CreateDescriptorSetLayout(PipelineType::RENDER);
#ifdef SHARED_MEM_RENDERING
  CreateComputePipeline(shaderPath + "Shaders/render_shared.spv",
                        PipelineType::RENDER, 4);
#else
  CreateComputePipeline(shaderPath + "Shaders/render.spv", PipelineType::RENDER,
                        4);
#endif
  createRenderTarget();
  SetupDescriptorSet(PipelineType::RENDER);
  UpdateAllDescriptorSets(PipelineType::RENDER);

#ifdef __APPLE__
  createRenderTarget();
  CreateDescriptorSetLayout(PipelineType::UPSAMPLING);
  CreateComputePipeline(shaderPath + "Shaders/upsample.spv",
                        PipelineType::UPSAMPLING, 3);
  SetupDescriptorSet(PipelineType::UPSAMPLING);
  UpdateAllDescriptorSets(PipelineType::UPSAMPLING);
#endif
  CreateSynchronization();

  // RecordAllCommandBuffers();
  std::cout << "\n=== Compute Pipeline Initialization Complete ===\n"
            << std::endl;
}

void ComputePipeline::CleanUp() {
  if (_vkContext.GetLogicalDevice() == VK_NULL_HANDLE) {
    return;
  }

  std::cout << "Cleaning up ComputePipeline..." << std::endl;

  for (auto &fence : _renderFences) {
    if (fence != VK_NULL_HANDLE) {
      vkDestroyFence(_vkContext.GetLogicalDevice(), fence, nullptr);
    }
  }
  _renderFences.clear();

  for (auto &fence : _preprocessFences) {
    if (fence != VK_NULL_HANDLE) {
      vkDestroyFence(_vkContext.GetLogicalDevice(), fence, nullptr);
    }
  }
  _preprocessFences.clear();

  for (auto &semaphore : _semaphores) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(_vkContext.GetLogicalDevice(), semaphore, nullptr);
    }
  }
  _semaphores.clear();

  _commandBuffers.clear();

  if (_computePipelines[PipelineType::DEBUG_RED_FILL] != VK_NULL_HANDLE) {
    vkDestroyPipeline(_vkContext.GetLogicalDevice(),
                      _computePipelines[PipelineType::DEBUG_RED_FILL], nullptr);
    _computePipelines[PipelineType::DEBUG_RED_FILL] = VK_NULL_HANDLE;
  }

  if (_pipelineLayouts[PipelineType::DEBUG_RED_FILL] != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(_vkContext.GetLogicalDevice(),
                            _pipelineLayouts[PipelineType::DEBUG_RED_FILL],
                            nullptr);
    _pipelineLayouts[PipelineType::DEBUG_RED_FILL] = VK_NULL_HANDLE;
  }

  // 6. Descriptor pool (this automatically frees all descriptor sets)
  if (_descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(_vkContext.GetLogicalDevice(), _descriptorPool,
                            nullptr);
    _descriptorPool = VK_NULL_HANDLE;
  }
  _descriptorSets.clear();

  if (_descriptorSetLayouts[PipelineType::DEBUG_RED_FILL] != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(
        _vkContext.GetLogicalDevice(),
        _descriptorSetLayouts[PipelineType::DEBUG_RED_FILL], nullptr);
    _descriptorSetLayouts[PipelineType::DEBUG_RED_FILL] = VK_NULL_HANDLE;
  }

  std::cout << "ComputePipeline cleanup complete" << std::endl;
}

void ComputePipeline::CreateCommandBuffers() {

  size_t size_sw = _vkContext.GetSwapchainImages().size();
  _commandBuffers.resize(size_sw);
  _renderCommandBuffers.resize(size_sw);
  VkCommandBufferAllocateInfo cbAllocInfo = {};
  cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAllocInfo.commandPool = _vkContext.GetCommandPool();
  cbAllocInfo.level =
      VK_COMMAND_BUFFER_LEVEL_PRIMARY; // executed by queue--secondary-> by
                                       // other buffers

  cbAllocInfo.commandBufferCount = static_cast<uint32_t>(size_sw);

  if (vkAllocateCommandBuffers(_vkContext.GetLogicalDevice(), &cbAllocInfo,
                               _commandBuffers.data()) != VK_SUCCESS ||
      vkAllocateCommandBuffers(_vkContext.GetLogicalDevice(), &cbAllocInfo,
                               _renderCommandBuffers.data()) != VK_SUCCESS)
    throw std::runtime_error("Fail to allocate buffers");

  std::cout << " Command buffer allocated: " << size_sw << std::endl;
}

void ComputePipeline::CreateSynchronization() {
  size_t images = _vkContext.GetSwapchainImages().size();
  _semaphores.resize(frames_in_flight); // gpu-gpu
  _renderSemaphores.resize(images);
  _preprocessFences.resize(frames_in_flight); // cpu-gpu
  _renderFences.resize(frames_in_flight);

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < frames_in_flight; i++) {
    if (vkCreateSemaphore(_vkContext.GetLogicalDevice(), &semaphoreInfo,
                          nullptr, &_semaphores[i]) != VK_SUCCESS ||
        vkCreateFence(_vkContext.GetLogicalDevice(), &fenceInfo, nullptr,
                      &_preprocessFences[i]) != VK_SUCCESS ||
        vkCreateFence(_vkContext.GetLogicalDevice(), &fenceInfo, nullptr,
                      &_renderFences[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create per-frame sync objects!");
    }
  }
  for (size_t i = 0; i < images; i++) {
    if (vkCreateSemaphore(_vkContext.GetLogicalDevice(), &semaphoreInfo,
                          nullptr, &_renderSemaphores[i]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create per-image render semaphores!");
    }
  }
  std::cout << " Created Sempahores and Fences " << std::endl;
}

void ComputePipeline::CreateDescriptorSetLayout(const PipelineType pType) {

  std::vector<VkDescriptorSetLayoutBinding> vulkanBindings;
  vulkanBindings.reserve(SHADER_LAYOUTS[pType].size());

  for (const auto &binding : SHADER_LAYOUTS[pType]) {

    VkDescriptorSetLayoutBinding vulkanBinding = {};
    vulkanBinding.binding = binding.binding;
    vulkanBinding.descriptorType = binding.type;
    vulkanBinding.descriptorCount = binding.count;
    vulkanBinding.stageFlags = binding.stageFlags;
    vulkanBinding.pImmutableSamplers = nullptr;

    vulkanBindings.push_back(vulkanBinding);
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(vulkanBindings.size());
  layoutInfo.pBindings = vulkanBindings.data();

  if (vkCreateDescriptorSetLayout(_vkContext.GetLogicalDevice(), &layoutInfo,
                                  nullptr, &_descriptorSetLayouts[pType]) !=
      VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor set layout!");
  }

  std::cout << " Descriptor set layout created (" << vulkanBindings.size()
            << " bindings)" << std::endl;
}

void ComputePipeline::CreateComputePipeline(std::string shaderName,
                                            const PipelineType pType,
                                            int numPushConstants) {
  std::cout << "  - Loading and creating compute pipeline..." << std::endl;
  auto computeShaderCode = ReadFile(shaderName);
  VkShaderModule computeShader = CreateShaderModule(computeShaderCode);

  VkPipelineShaderStageCreateInfo computeShaderStageInfo = {};
  computeShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  computeShaderStageInfo.module = computeShader;
  computeShaderStageInfo.pName = "main";

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &_descriptorSetLayouts[pType];

  VkPushConstantRange pushConstantRange = {};
  if (numPushConstants != 0) {

    pushConstantRange.stageFlags =
        VK_SHADER_STAGE_COMPUTE_BIT; // Should be 0x00000020
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t) * numPushConstants;

    std::cout << "Push constant range - stage: " << pushConstantRange.stageFlags
              << ", offset: " << pushConstantRange.offset
              << ", size: " << pushConstantRange.size << std::endl;

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
  } else {
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;
  }

  if (vkCreatePipelineLayout(_vkContext.GetLogicalDevice(), &pipelineLayoutInfo,
                             nullptr, &_pipelineLayouts[pType]) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout!");
  }

  VkComputePipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.layout = _pipelineLayouts[pType];
  pipelineInfo.stage = computeShaderStageInfo;

  if (vkCreateComputePipelines(_vkContext.GetLogicalDevice(), VK_NULL_HANDLE, 1,
                               &pipelineInfo, nullptr,
                               &_computePipelines[pType]) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create compute pipeline!");
  }
  vkDestroyShaderModule(_vkContext.GetLogicalDevice(), computeShader, nullptr);
  std::cout << "Compute pipeline created for pipeline type " << (int)pType
            << " using shader: " << shaderName << std::endl;
}

void ComputePipeline::SetupDescriptorSet(const PipelineType pType) {
  std::cout << "  - Setting up descriptor sets for pipeline type " << (int)pType
            << "..." << std::endl;

  uint32_t swapchainImageCount =
      static_cast<uint32_t>(_vkContext.GetSwapchainImages().size());

  _descriptorSets[pType].resize(swapchainImageCount);

  std::vector<VkDescriptorSetLayout> layouts;

  if (pType == PipelineType::RADIX_HISTOGRAM_1) {
    layouts = std::vector<VkDescriptorSetLayout>(
        swapchainImageCount,
        _descriptorSetLayouts[PipelineType::RADIX_HISTOGRAM_0]);
  } else if (pType == PipelineType::RADIX_SCATTER_1) {
    layouts = std::vector<VkDescriptorSetLayout>(
        swapchainImageCount,
        _descriptorSetLayouts[PipelineType::RADIX_SCATTER_0]);
  } else {
    layouts = std::vector<VkDescriptorSetLayout>(swapchainImageCount,
                                                 _descriptorSetLayouts[pType]);
  }

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = _descriptorPool;
  allocInfo.descriptorSetCount = swapchainImageCount;
  allocInfo.pSetLayouts = layouts.data();

  if (vkAllocateDescriptorSets(_vkContext.GetLogicalDevice(), &allocInfo,
                               _descriptorSets[pType].data()) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate descriptor sets!");
  }

  std::cout << " Allocated " << swapchainImageCount
            << " descriptor sets for pipeline type " << (int)pType << std::endl;
}

void ComputePipeline::RecordCommandPreprocess(uint32_t imageIndex) {
  VkCommandBuffer commandBuffer = _commandBuffers[imageIndex];

  // Begin recording
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0; // Not one-time submit

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("Failed to begin recording command buffer!");
  }
  /////////////////////////////////////////////////////////////////////////////////////
  // Transition image to GENERAL
  static std::vector<bool> firstFrame(_vkContext.GetSwapchainImages().size(),
                                      true);

  VkImageLayout srcLayout = firstFrame[imageIndex]
                                ? VK_IMAGE_LAYOUT_UNDEFINED
                                : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  firstFrame[imageIndex] = false;

  TransitionImage(commandBuffer, srcLayout, // Use correct source layout
                  VK_IMAGE_LAYOUT_GENERAL,
                  _vkContext.GetSwapchainImages()[imageIndex].image,
                  VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

  /////////////////////////////////////////////////////////////////////////////////////
  // Bind pipeline 1

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    _computePipelines[PipelineType::PREPROCESS]);

  struct {
    int32_t numGauss;
    float nearPlane;
    float farPlane;
    uint32_t culling;
  } pushPreprocess = {_numGaussians, g_renderSettings.nearPlane,
                      g_renderSettings.farPlane,
                      uint32_t(g_renderSettings.enableCulling)};
  vkCmdPushConstants(commandBuffer, _pipelineLayouts[PipelineType::PREPROCESS],
                     VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushPreprocess),
                     &pushPreprocess);
  // Bind descriptor set
  vkCmdBindDescriptorSets(
      commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      _pipelineLayouts[PipelineType::PREPROCESS], 0, 1,
      &_descriptorSets[PipelineType::PREPROCESS][imageIndex], 0, nullptr);

  uint32_t groupX = (_numGaussians + 255) / 256;
  vkCmdDispatch(commandBuffer, groupX, 1, 1);

  /////////////////////////////////////////////////////////////////////////////////////
  // Barrier1
  VkMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0,
                       nullptr, 0, nullptr);

  ///////////////////////////////////////////////////////////////////////////////////////
  //// Prefix Sum

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    _computePipelines[PipelineType::PREFIXSUM]);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          _pipelineLayouts[PipelineType::PREFIXSUM], 0, 1,
                          &_descriptorSets[PipelineType::PREFIXSUM][imageIndex],
                          0, nullptr);

  uint32_t prefixSumGroups = (_numGaussians + 255) / 256;

  for (uint32_t step = 0; step <= _numSteps; step++) {
    struct PushConstants {
      uint32_t step;
      int32_t numElements;
      int32_t readFromA;
    } pushConstants = {step, _numGaussians, (step % 2) == 0 ? 1 : 0};

    vkCmdPushConstants(commandBuffer, _pipelineLayouts[PipelineType::PREFIXSUM],
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants),
                       &pushConstants);
    vkCmdDispatch(commandBuffer, prefixSumGroups, 1, 1);

    if (step < _numSteps - 1) {
      VkMemoryBarrier stepBarrier = {};
      stepBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      stepBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      stepBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                           &stepBarrier, 0, nullptr, 0, nullptr);
    }
  }

  VkBufferCopy copyRegion = {};
  copyRegion.srcOffset = (_numGaussians - 1) * sizeof(uint32_t);
  copyRegion.dstOffset = 0;
  copyRegion.size = sizeof(uint32_t);

  vkCmdCopyBuffer(commandBuffer, _resultBufferPrefix,
                  _gaussianBuffers.numRendered.staging, 1, &copyRegion);

  ///////////////////// END PREFIX SUM /////////////////////

  /////////////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////////////
  // End recording
  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to record command buffer!");
  }

  /*std::cout << " Command buffer recorded for swapchain image " << imageIndex
            << std::endl;*/
}

void ComputePipeline::RecordCommandRender(uint32_t imageIndex, int numRendered,
                                          Camera &cam) {
  VkCommandBuffer commandBuffer = _renderCommandBuffers[imageIndex];

  // Begin recording
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0; // Not one-time submit

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    throw std::runtime_error("Failed to begin recording command buffer!");
  }

  if (numRendered) {

#ifdef __APPLE__
    TransitionImage(
        commandBuffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // From previous frame
        VK_IMAGE_LAYOUT_GENERAL,                  // For compute write
        _renderTarget.image, VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
#endif
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      _computePipelines[PipelineType::ASSIGN_TILE_IDS]);

    VkExtent2D extent = _vkContext.GetSwapchainExtent();
    uint32_t tileX = (extent.width / _windowResize + 15) / 16;
    struct {
      uint32_t tile;
      int32_t nGauss;
      uint32_t culling;
    } pushCt = {tileX, _numGaussians, 1};
    vkCmdPushConstants(commandBuffer,
                       _pipelineLayouts[PipelineType::ASSIGN_TILE_IDS],
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushCt), &pushCt);

    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        _pipelineLayouts[PipelineType::ASSIGN_TILE_IDS], 0, 1,
        &_descriptorSets[PipelineType::ASSIGN_TILE_IDS][imageIndex], 0,
        nullptr);

    vkCmdDispatch(commandBuffer, (_numGaussians + 255) / 256, 1, 1);

    VkMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // For presentation

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1, &barrier,
                         0, nullptr, 0, nullptr);

    ////////////////////////////////////////////////////////////////////////////////////////

    VkMemoryBarrier barrier_t = {};
    barrier_t.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier_t.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier_t.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // For presentation

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1, &barrier_t,
                         0, nullptr, 0, nullptr);
    uint32_t numElementsToSort = numRendered;

    uint32_t elementsPerWorkgroup =
        WORKGROUP_SIZE * blocks_per_workgroup; // 256 * 32 = 8192
    uint32_t numWorkgroups =
        (numElementsToSort + elementsPerWorkgroup - 1) / elementsPerWorkgroup;

    // Prepare push constants
    struct RadixPushConstants {
      uint32_t g_num_elements;
      uint32_t g_shift;
      uint32_t g_num_workgroups;
      uint32_t g_num_blocks_per_workgroup;
    } radixPC;

    radixPC.g_num_elements = numElementsToSort;
    radixPC.g_num_workgroups = numWorkgroups;
    radixPC.g_num_blocks_per_workgroup = blocks_per_workgroup;

    // Perform 6 passes of radix sort (tiles_ID always can be represented with
    // 2 bits)
    for (uint32_t pass = 0; pass < 6; pass++) {
      radixPC.g_shift = pass * 8;

      bool isEven = (pass % 2 == 0);

      // HISTOGRAM PASS
      PipelineType histType = isEven ? PipelineType::RADIX_HISTOGRAM_0
                                     : PipelineType::RADIX_HISTOGRAM_1;

      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                        _computePipelines[PipelineType::RADIX_HISTOGRAM_0]);
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                              _pipelineLayouts[PipelineType::RADIX_HISTOGRAM_0],
                              0, 1, &_descriptorSets[histType][imageIndex], 0,
                              nullptr);
      vkCmdPushConstants(
          commandBuffer, _pipelineLayouts[PipelineType::RADIX_HISTOGRAM_0],
          VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RadixPushConstants), &radixPC);
      vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);

      VkMemoryBarrier histBarrier = {};
      histBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
      histBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
      histBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                           &histBarrier, 0, nullptr, 0, nullptr);

      // SCATTER PASS
      PipelineType scatterType = isEven ? PipelineType::RADIX_SCATTER_0
                                        : PipelineType::RADIX_SCATTER_1;

      vkCmdBindPipeline(
          commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
          _computePipelines[PipelineType::RADIX_SCATTER_0]); // Use same
                                                             // pipeline
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                              _pipelineLayouts[PipelineType::RADIX_SCATTER_0],
                              0, 1, &_descriptorSets[scatterType][imageIndex],
                              0, nullptr);
      vkCmdPushConstants(
          commandBuffer, _pipelineLayouts[PipelineType::RADIX_SCATTER_0],
          VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RadixPushConstants), &radixPC);
      vkCmdDispatch(commandBuffer, numWorkgroups, 1, 1);

      if (pass < 5) {
        VkMemoryBarrier scatterBarrier = {};
        scatterBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        scatterBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        scatterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                             &scatterBarrier, 0, nullptr, 0, nullptr);
      }
    }

    VkMemoryBarrier finalSortBarrier = {};
    finalSortBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    finalSortBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    finalSortBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                         &finalSortBarrier, 0, nullptr, 0, nullptr);

    /////////////////////////////////////////////////////////////////////////////////////////
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      _computePipelines[PipelineType::TILE_BOUNDARIES]);

    vkCmdPushConstants(
        commandBuffer, _pipelineLayouts[PipelineType::TILE_BOUNDARIES],
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &numRendered);

    // Bind descriptor set
    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        _pipelineLayouts[PipelineType::TILE_BOUNDARIES], 0, 1,
        &_descriptorSets[PipelineType::TILE_BOUNDARIES][imageIndex], 0,
        nullptr);

    vkCmdDispatch(commandBuffer, (numRendered + 255) / 256, 1, 1);

    VkMemoryBarrier tilesBarrier = {};
    tilesBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    tilesBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    tilesBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1,
                         &tilesBarrier, 0, nullptr, 0, nullptr);

    ///////////////////////////////////////////////////////////////////////////////////////////
    clearSwapchain(commandBuffer, imageIndex, true);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      _computePipelines[PipelineType::RENDER]);

    struct {
      uint32_t w;
      uint32_t h;
      uint32_t wireframe;
      float gaussScale;
    } pcRender = {extent.width / _windowResize, extent.height / _windowResize,
                  uint32_t(g_renderSettings.showWireframe),
                  g_renderSettings.gaussianScale};

    vkCmdPushConstants(commandBuffer, _pipelineLayouts[PipelineType::RENDER],
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcRender),
                       &pcRender);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            _pipelineLayouts[PipelineType::RENDER], 0, 1,
                            &_descriptorSets[PipelineType::RENDER][imageIndex],
                            0, nullptr);

    vkCmdDispatch(commandBuffer, (extent.width / _windowResize + 15) / 16,
                  (extent.height / _windowResize + 15) / 16, 1);
    ///////////////////////////////////////////////////////////////////////////////////////
    VkMemoryBarrier barrier_x = {};
    barrier_x.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier_x.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier_x.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1, &barrier_x,
                         0, nullptr, 0, nullptr);

/////////////////////////////////////////////////////////////////////////
// Apple upsampling
#ifdef __APPLE__
    TransitionImage(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    _renderTarget.image, VK_ACCESS_SHADER_WRITE_BIT,
                    VK_ACCESS_SHADER_READ_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      _computePipelines[PipelineType::UPSAMPLING]);

    vkCmdBindDescriptorSets(
        commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        _pipelineLayouts[PipelineType::UPSAMPLING], 0, 1,
        &_descriptorSets[PipelineType::UPSAMPLING][imageIndex], 0, nullptr);
    vkCmdDispatch(commandBuffer, (extent.width + 15) / 16,
                  (extent.height + 15) / 16, 1);

    VkMemoryBarrier barrier_upsample = {};
    barrier_upsample.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier_upsample.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier_upsample.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1,
                         &barrier_upsample, 0, nullptr, 0, nullptr);
#endif
  } else {

    clearSwapchain(commandBuffer, imageIndex);
  }
  TransitionImage(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  _vkContext.GetSwapchainImages()[imageIndex].image,
                  VK_ACCESS_SHADER_WRITE_BIT,
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

  _graphicsPipeline.RecordAxisRenderPass(commandBuffer, imageIndex, cam);

  VkMemoryBarrier barrier_graphics = {};
  barrier_graphics.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
  barrier_graphics.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  barrier_graphics.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 1,
                       &barrier_graphics, 0, nullptr, 0, nullptr);
  // Record ImGui render pass
  RecordImGuiRenderPass(commandBuffer, imageIndex, cam);

  TransitionImage(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  _vkContext.GetSwapchainImages()[imageIndex].image,
                  VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_ACCESS_MEMORY_READ_BIT,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to record command buffer!");
  }
}

VkShaderModule
ComputePipeline::CreateShaderModule(const std::vector<char> &code) {
  VkShaderModuleCreateInfo shaderCreateInfo = {};
  shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderCreateInfo.codeSize = code.size();
  shaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(_vkContext.GetLogicalDevice(), &shaderCreateInfo,
                           nullptr, &shaderModule) != VK_SUCCESS)
    throw std::runtime_error("Fail Creating Shader Module");

  return shaderModule;
}

void ComputePipeline::TransitionImage(VkCommandBuffer commandBuffer,
                                      VkImageLayout in, VkImageLayout out,
                                      VkImage image, VkAccessFlags src,
                                      VkAccessFlags dst,
                                      VkPipelineStageFlagBits srcStage,
                                      VkPipelineStageFlagBits dstStage) {

  // Transition swapchain image to GENERAL layout for compute write
  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; // Type of barrier
  barrier.oldLayout = in;  // Don't care about previous contents
  barrier.newLayout = out; // Layout for compute shader access
  barrier.srcQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED; // Not transferring between queue families
  barrier.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED; // Not transferring between queue families
  barrier.image = image;       // The specific swapchain image
  barrier.subresourceRange.aspectMask =
      VK_IMAGE_ASPECT_COLOR_BIT;               // Color data (not depth/stencil)
  barrier.subresourceRange.baseMipLevel = 0;   // Mipmap level 0
  barrier.subresourceRange.levelCount = 1;     // Only 1 mipmap level
  barrier.subresourceRange.baseArrayLayer = 0; // Array layer 0
  barrier.subresourceRange.layerCount = 1;     // Only 1 array layer
  barrier.srcAccessMask = src;                 // No previous access to wait for
  barrier.dstAccessMask = dst;                 // Compute shader will write

  vkCmdPipelineBarrier(
      commandBuffer, // Command buffer to record into
      srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
      &barrier); // 1 image barrier, no memory/buffer barriers //
}

void ComputePipeline::RenderFrame(Camera &cam) {

  vkWaitForFences(_vkContext.GetLogicalDevice(), 1,
                  &_renderFences[_currentFrame], VK_TRUE, UINT64_MAX);

  vkResetFences(_vkContext.GetLogicalDevice(), 1,
                &_preprocessFences[_currentFrame]);

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      _vkContext.GetLogicalDevice(), _vkContext.GetSwapchain(), UINT64_MAX,
      _semaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to acquire swapchain image!");
  }

  vkResetCommandBuffer(_commandBuffers[imageIndex], 0);
  vkResetCommandBuffer(_renderCommandBuffers[imageIndex], 0);
  RecordCommandPreprocess(imageIndex);
  submitCommandBuffer(imageIndex);

  vkWaitForFences(_vkContext.GetLogicalDevice(), 1,
                  &_preprocessFences[_currentFrame], VK_TRUE, UINT64_MAX);

  uint32_t totalRendered = ReadFinalPrefixSum();
  g_renderSettings.numRendered = totalRendered;

  if (totalRendered > _sizeBufferMax) {
    resizeBuffers(totalRendered * 1.25f);
  }

  vkResetFences(_vkContext.GetLogicalDevice(), 1,
                &_renderFences[_currentFrame]);

  RecordCommandRender(imageIndex, totalRendered, cam);
  submitCommandBuffer(imageIndex, false);

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  VkSemaphore waitSemaphores[] = {_renderSemaphores[imageIndex]};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = waitSemaphores;
  presentInfo.swapchainCount = 1;
  VkSwapchainKHR swapchains[] = {_vkContext.GetSwapchain()};
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;

  vkQueuePresentKHR(_vkContext.GetGraphicsQueue(), &presentInfo);
  _currentFrame = (_currentFrame + 1) % _renderFences.size();
}

void ComputePipeline::CreateDescriptorPool() {
  std::map<VkDescriptorType, uint32_t> typeCounts;
  uint32_t swapchainImageCount =
      static_cast<uint32_t>(_vkContext.GetSwapchainImages().size());

  for (const auto &[pipelineType, bindings] : SHADER_LAYOUTS) {
    for (const auto &binding : bindings) {
      typeCounts[binding.type] += binding.count * swapchainImageCount;
    }
  }

  std::vector<VkDescriptorPoolSize> poolSizes;
  for (const auto &[type, count] : typeCounts) {
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = type;
    poolSize.descriptorCount = count;
    poolSizes.push_back(poolSize);
  }

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();

  poolInfo.maxSets =
      static_cast<uint32_t>(SHADER_LAYOUTS.size()) * swapchainImageCount;

  if (vkCreateDescriptorPool(_vkContext.GetLogicalDevice(), &poolInfo, nullptr,
                             &_descriptorPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor pool!");
  }

  std::cout << " Descriptor Pool created with " << poolSizes.size()
            << " different Pool Size(s)" << std::endl;
}

void ComputePipeline::UpdateAllDescriptorSets(const PipelineType pType) {
  std::cout << "  - Updating descriptor sets with swapchain images..."
            << std::endl;

  auto &swapchainImages = _vkContext.GetSwapchainImages();

  for (const auto &descriptor : SHADER_LAYOUTS[pType]) {
    for (uint32_t i = 0; i < swapchainImages.size(); i++) {
      if (descriptor.type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        VkImageView imageView = swapchainImages[i].imageView;

#ifdef __APPLE__
        if (pType == PipelineType::RENDER)
          imageView = _renderTarget.view;
#endif

        BindImageToDescriptor(pType, i, imageView, descriptor.binding);

      } else if (descriptor.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        BindSamplerToDescriptor(pType, i, _renderTarget.view,
                                _renderTarget.sampler, descriptor.binding);
      } else {
        BindBufferToDescriptor(pType, descriptor.binding, i,
                               GetBufferByName(descriptor.name),
                               descriptor.type);
      }
    }
  }
}

VkBuffer ComputePipeline::GetBufferByName(const std::string &bufferName) {
  // Map is better XD
  if (bufferName == "xyz")
    return _gaussianBuffers.xyz;
  if (bufferName == "scales")
    return _gaussianBuffers.scales;
  if (bufferName == "rotations")
    return _gaussianBuffers.rotations;
  if (bufferName == "opacity")
    return _gaussianBuffers.opacity;
  if (bufferName == "sh")
    return _gaussianBuffers.sh;
  if (bufferName == "camUniform")
    return _gaussianBuffers.camUniform;
  if (bufferName == "radii")
    return _gaussianBuffers.radii;
  if (bufferName == "depths")
    return _gaussianBuffers.depth;
  if (bufferName == "rgb")
    return _gaussianBuffers.color;
  if (bufferName == "conicOpacity")
    return _gaussianBuffers.conicOpacity;
  if (bufferName == "pointsXY")
    return _gaussianBuffers.points2d;
  if (bufferName == "tilesTouched")
    return _gaussianBuffers.tilesTouched;
  if (bufferName == "tilesTouchedPrefixSum")
    return _gaussianBuffers.tilesTouchedPrefixSum;
  if (bufferName == "boundingBox")
    return _gaussianBuffers.boundingBox;
  if (bufferName == "keys")
    return _gaussianBuffers.keys;
  if (bufferName == "values")
    return _gaussianBuffers.values;
  if (bufferName == "keysRadix")
    return _gaussianBuffers.keysRadix;
  if (bufferName == "valuesRadix")
    return _gaussianBuffers.valuesRadix;
  if (bufferName == "ranges")
    return _gaussianBuffers.ranges;
  if (bufferName == "prefixResult")
    return _resultBufferPrefix;
  if (bufferName == "histograms")
    return _gaussianBuffers.histogram;

  throw std::runtime_error("Unknown buffer name: " + bufferName);
}

void ComputePipeline::submitCommandBuffer(uint32_t imageIndex, bool waitSem) {
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; // Type of submit
  submitInfo.commandBufferCount = 1; // Number of command buffers to submit

  VkSemaphore waitSemaphores[] = {_semaphores[_currentFrame]};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
  VkSemaphore signalSemaphores[] = {_renderSemaphores[imageIndex]};
  if (waitSem) {
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
  } else {

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
  }
  VkFence fence = (waitSem) ? _preprocessFences[_currentFrame]
                            : _renderFences[_currentFrame];

  VkCommandBuffer *bufferToSubmit = waitSem
                                        ? &_commandBuffers[imageIndex]
                                        : &_renderCommandBuffers[imageIndex];

  submitInfo.pCommandBuffers = bufferToSubmit;

  if (vkQueueSubmit(_vkContext.GetGraphicsQueue(), // Queue to
                                                   // submit to
                    1,                             // Number of submits
                    &submitInfo,                   // Submit info
                    fence) != VK_SUCCESS) {        // Fence to signal when done
    throw std::runtime_error("Failed to submit compute command buffer!");
  }
}

int ComputePipeline::getRadixIterations() {
  VkExtent2D extent = _vkContext.GetSwapchainExtent();
  uint32_t nTiles = ((extent.width + 15) / 16) * ((extent.height + 15) / 16);
  uint32_t tileBits = static_cast<uint32_t>(std::ceil(std::log2(nTiles)));
  uint32_t totalBits = 32 + tileBits;
  return (totalBits + 7) / 8;
}

void ComputePipeline::resizeBuffers(float size) {
  // We could implement memory Pool?
  VkPhysicalDevice physicalDevice = _vkContext.GetPhysicalDevice();
  VkDevice device = _vkContext.GetLogicalDevice();

  _buffManager->DestroyBuffer(device, _gaussianBuffers.valuesRadix);
  _buffManager->DestroyBuffer(device, _gaussianBuffers.values);
  _buffManager->DestroyBuffer(device, _gaussianBuffers.keys);
  _buffManager->DestroyBuffer(device, _gaussianBuffers.keysRadix);
  _buffManager->DestroyBuffer(device, _gaussianBuffers.histogram);

  VkDeviceSize bufferSizeKey = sizeof(int64_t) * int(size);
  VkDeviceSize bufferSizeValue = sizeof(int32_t) * int(size);

  uint32_t elementsPerWorkgroup =
      WORKGROUP_SIZE * blocks_per_workgroup; // 256 * 32 = 8192
  uint32_t numWorkgroups =
      (int(size) + elementsPerWorkgroup - 1) / elementsPerWorkgroup;
  VkDeviceSize histogramSize =
      RADIX_SORT_BINS * numWorkgroups * sizeof(uint32_t);

  VkBufferUsageFlags usage =
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

  _gaussianBuffers.keys =
      _buffManager->CreateBuffer(device, physicalDevice, bufferSizeKey, usage,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  _gaussianBuffers.keysRadix =
      _buffManager->CreateBuffer(device, physicalDevice, bufferSizeKey, usage,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  _gaussianBuffers.values =
      _buffManager->CreateBuffer(device, physicalDevice, bufferSizeValue, usage,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  _gaussianBuffers.valuesRadix =
      _buffManager->CreateBuffer(device, physicalDevice, bufferSizeValue, usage,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  _gaussianBuffers.histogram =
      _buffManager->CreateBuffer(device, physicalDevice, histogramSize, usage,
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  _sizeBufferMax = uint32_t(size);
  UpdateAllDescriptorSets(PipelineType::ASSIGN_TILE_IDS);
  UpdateAllDescriptorSets(PipelineType::RADIX_HISTOGRAM_0);
  UpdateAllDescriptorSets(PipelineType::RADIX_HISTOGRAM_1);
  UpdateAllDescriptorSets(PipelineType::RADIX_SCATTER_0);
  UpdateAllDescriptorSets(PipelineType::RADIX_SCATTER_1);
  UpdateAllDescriptorSets(PipelineType::TILE_BOUNDARIES);
  UpdateAllDescriptorSets(PipelineType::RENDER);

  std::cout << "resize Buffers and update Descriptors" << std::endl;
}

void ComputePipeline::SetUpRadixBuffers() {}

void ComputePipeline::RecordImGuiRenderPass(VkCommandBuffer commandBuffer,
                                            uint32_t imageIndex, Camera &cam) {

  _imGuiHandler.NewFrame();
  _imGuiHandler.CreateUI(cam);

  _imGuiHandler.RecordImGuiRenderPass(commandBuffer, imageIndex);
}

void ComputePipeline::clearSwapchain(VkCommandBuffer commandBuffer,
                                     uint32_t imageIndex, bool toGeneral) {

  TransitionImage(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  _vkContext.GetSwapchainImages()[imageIndex].image,
                  VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT);

  VkClearColorValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};
  VkImageSubresourceRange range = {};
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel = 0;
  range.levelCount = 1;
  range.baseArrayLayer = 0;
  range.layerCount = 1;

  vkCmdClearColorImage(
      commandBuffer, _vkContext.GetSwapchainImages()[imageIndex].image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

  TransitionImage(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_GENERAL,
                  _vkContext.GetSwapchainImages()[imageIndex].image,
                  VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void ComputePipeline::createRenderTarget() {

  // Get swapchain extent
  VkExtent2D extent = _vkContext.GetSwapchainExtent();
  uint32_t width = extent.width / _windowResize;
  uint32_t height = extent.height / _windowResize;

  // Create image
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.extent = {width, height, 1};
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vkCreateImage(_vkContext.GetLogicalDevice(), &imageInfo, nullptr,
                &_renderTarget.image);

  // Allocate memory
  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(_vkContext.GetLogicalDevice(),
                               _renderTarget.image, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(
      memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  vkAllocateMemory(_vkContext.GetLogicalDevice(), &allocInfo, nullptr,
                   &_renderTarget.memory);
  vkBindImageMemory(_vkContext.GetLogicalDevice(), _renderTarget.image,
                    _renderTarget.memory, 0);

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = _renderTarget.image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
  viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  vkCreateImageView(_vkContext.GetLogicalDevice(), &viewInfo, nullptr,
                    &_renderTarget.view);
 
  std::cout << "DEBUG: Created ImageView handle: " << std::hex << _renderTarget.view << std::dec << std::endl;
  // Create sampler for reading the image as texture
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.anisotropyEnable = VK_FALSE;
  samplerInfo.maxAnisotropy = 1.0f;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = 0.0f;

  vkCreateSampler(_vkContext.GetLogicalDevice(), &samplerInfo, nullptr,
                  &_renderTarget.sampler);

  transitionRenderTargetLayout(VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ComputePipeline::transitionRenderTargetLayout(VkImageLayout oldLayout,
                                                   VkImageLayout newLayout) {
  // Allocate a new command buffer
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = _vkContext.GetCommandPool();
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(_vkContext.GetLogicalDevice(), &allocInfo,
                           &commandBuffer);

  // Begin recording
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  // Use your TransitionImage function
  TransitionImage(commandBuffer, oldLayout, newLayout, _renderTarget.image,
                  VK_ACCESS_NONE,                        // src access mask
                  VK_ACCESS_SHADER_WRITE_BIT,            // dst access mask
                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,     // src stage
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT); // dst stage

  // End recording
  vkEndCommandBuffer(commandBuffer);

  // Submit and wait
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(_vkContext.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(_vkContext.GetGraphicsQueue());

  // Free the command buffer
  vkFreeCommandBuffers(_vkContext.GetLogicalDevice(),
                       _vkContext.GetCommandPool(), 1, &commandBuffer);
}

uint32_t ComputePipeline::findMemoryType(uint32_t typeFilter,
                                         VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(_vkContext.GetPhysicalDevice(),
                                      &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find suitable memory type!");
}
void ComputePipeline::RecordAllCommandBuffers() {}

void ComputePipeline::BindImageToDescriptor(const PipelineType pType,
                                            uint32_t i, VkImageView view,
                                            uint32_t binding) {
  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  imageInfo.imageView = view;
  imageInfo.sampler = VK_NULL_HANDLE;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = _descriptorSets[pType][i]; // Specific descriptor set
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(_vkContext.GetLogicalDevice(), 1, &descriptorWrite, 0,
                         nullptr);

  std::cout << "Descriptor set " << i << "  Swapchain image " << i << std::endl;
}

void ComputePipeline::BindBufferToDescriptor(const PipelineType pType,
                                             uint32_t bindingIndex, uint32_t i,
                                             VkBuffer buffer,
                                             VkDescriptorType descriptorType) {
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = (descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                         ? sizeof(CameraUniforms)
                         : VK_WHOLE_SIZE;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = _descriptorSets[pType][i];
  descriptorWrite.dstBinding = bindingIndex;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = descriptorType;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &bufferInfo;

  vkUpdateDescriptorSets(_vkContext.GetLogicalDevice(), 1, &descriptorWrite, 0,
                         nullptr);
}

void ComputePipeline::BindSamplerToDescriptor(const PipelineType pType,
                                              uint32_t i, VkImageView view,
                                              VkSampler sampler,
                                              uint32_t binding) {
  VkDescriptorImageInfo imageInfo = {};
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.imageView = view;
  imageInfo.sampler = sampler;

  VkWriteDescriptorSet descriptorWrite = {};
  descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptorWrite.dstSet = _descriptorSets[pType][i];
  descriptorWrite.dstBinding = binding;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pImageInfo = &imageInfo;

  vkUpdateDescriptorSets(_vkContext.GetLogicalDevice(), 1, &descriptorWrite, 0,
                         nullptr);
}
