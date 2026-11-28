#include <renderer/pipelines/main_pipeline.hpp>

#include <renderer/vertex.hpp>
#include <renderer/ubo.hpp>
#include <renderer/push.hpp>
#include <renderer/texture.hpp>
#include <array>
#include <vector>

MainPipeline::MainPipeline(GpuContext &context, GpuSwapchain &swapchain, GpuResource &resource)
    : GpuPipeline(context, swapchain, resource, "slang.spv")
{
  initialize();
  entityPipeline = createEntityPipeline();
}

vk::raii::PipelineLayout MainPipeline::createPipelineLayout()
{
  vk::PushConstantRange pushRange{
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .offset = 0,
      .size = sizeof(PushConstants),
  };

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
      .setLayoutCount = 1,
      .pSetLayouts = &*descriptorSetLayout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushRange,
  };

  return vk::raii::PipelineLayout(gpuContext.getDevice(), pipelineLayoutInfo);
}

vk::raii::Pipeline MainPipeline::createPipeline()
{
  std::array<vk::VertexInputBindingDescription, 2> bindings{{
      {
          .binding = 0,
          .stride = sizeof(ChunkVertex),
          .inputRate = vk::VertexInputRate::eVertex,
      },
      {
          .binding = 1,
          .stride = sizeof(ChunkInstance),
          .inputRate = vk::VertexInputRate::eInstance,
      },
  }};

  std::array<vk::VertexInputAttributeDescription, 3> attributes{{
      {
          .location = 0,
          .binding = 0,
          .format = vk::Format::eR32Uint,
          .offset = offsetof(ChunkVertex, data),
      },
      {
          .location = 1,
          .binding = 0,
          .format = vk::Format::eR32Uint,
          .offset = offsetof(ChunkVertex, textureId),
      },
      {
          .location = 2,
          .binding = 1,
          .format = vk::Format::eR32G32B32A32Sfloat,
          .offset = offsetof(ChunkInstance, origin),
      },
  }};

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size()),
      .pVertexBindingDescriptions = bindings.data(),
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
      .pVertexAttributeDescriptions = attributes.data(),
  };

  return buildPipeline(vertexInputInfo, "vertMain");
}

vk::raii::Pipeline MainPipeline::createEntityPipeline()
{
  vk::VertexInputBindingDescription binding{
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = vk::VertexInputRate::eVertex,
  };

  std::array<vk::VertexInputAttributeDescription, 4> attributes{{
      {
          .location = 0,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = offsetof(Vertex, position),
      },
      {
          .location = 1,
          .binding = 0,
          .format = vk::Format::eR32G32Sfloat,
          .offset = offsetof(Vertex, texCoord),
      },
      {
          .location = 2,
          .binding = 0,
          .format = vk::Format::eR32G32B32Sfloat,
          .offset = offsetof(Vertex, normal),
      },
      {
          .location = 3,
          .binding = 0,
          .format = vk::Format::eR32Uint,
          .offset = offsetof(Vertex, textureId),
      },
  }};

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
      .vertexBindingDescriptionCount = 1,
      .pVertexBindingDescriptions = &binding,
      .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
      .pVertexAttributeDescriptions = attributes.data(),
  };

  return buildPipeline(vertexInputInfo, "entityVertMain");
}

vk::raii::Pipeline MainPipeline::buildPipeline(const vk::PipelineVertexInputStateCreateInfo &vertexInputInfo, const char *vertexEntry)
{
  vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eVertex,
      .module = shader.getShaderModule(),
      .pName = vertexEntry,
  };

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
      .stage = vk::ShaderStageFlagBits::eFragment,
      .module = shader.getShaderModule(),
      .pName = "fragMain",
  };

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
      .topology = vk::PrimitiveTopology::eTriangleList,
  };

  std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::ePrimitiveTopology};
  vk::PipelineDynamicStateCreateInfo dynamicState{
      .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
      .pDynamicStates = dynamicStates.data(),
  };

  vk::PipelineViewportStateCreateInfo viewportState{
      .viewportCount = 1,
      .scissorCount = 1,
  };

  vk::PipelineRasterizationStateCreateInfo rasterizer{
      .depthClampEnable = vk::False,
      .rasterizerDiscardEnable = vk::False,
      .polygonMode = vk::PolygonMode::eFill,
      .cullMode = vk::CullModeFlagBits::eBack,
      .frontFace = vk::FrontFace::eCounterClockwise,
      .depthBiasEnable = vk::False,
      .lineWidth = 1.0f,
  };

  vk::PipelineMultisampleStateCreateInfo multisampling{
      .rasterizationSamples = gpuContext.getMsaaSamples(),
      .sampleShadingEnable = vk::True,
      .minSampleShading = 0.2f,
  };

  vk::PipelineColorBlendAttachmentState colorBlendAttachment{
      .blendEnable = vk::False,
      .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
  };

  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable = vk::False,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
  };

  vk::PipelineDepthStencilStateCreateInfo depthStencil{
      .depthTestEnable = vk::True,
      .depthWriteEnable = vk::True,
      .depthCompareOp = vk::CompareOp::eGreater,
      .depthBoundsTestEnable = vk::False,
      .stencilTestEnable = vk::False,
  };

  vk::StructureChain<
      vk::GraphicsPipelineCreateInfo,
      vk::PipelineRenderingCreateInfo>
      pipelineCreateInfoChain = {
          {
              .stageCount = 2,
              .pStages = shaderStages,
              .pVertexInputState = &vertexInputInfo,
              .pInputAssemblyState = &inputAssembly,
              .pViewportState = &viewportState,
              .pRasterizationState = &rasterizer,
              .pMultisampleState = &multisampling,
              .pDepthStencilState = &depthStencil,
              .pColorBlendState = &colorBlending,
              .pDynamicState = &dynamicState,
              .layout = pipelineLayout,
              .renderPass = nullptr,
          },
          {.colorAttachmentCount = 1,
           .pColorAttachmentFormats = &gpuSwapchain.getSurfaceFormat().format,
           .depthAttachmentFormat = findDepthFormat()},
      };

  return vk::raii::Pipeline(gpuContext.getDevice(), nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

vk::raii::DescriptorSetLayout MainPipeline::createDescriptorSetLayout()
{
  std::array<vk::DescriptorSetLayoutBinding, 2> bindings{{
      {
          .binding = 0,
          .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
          .descriptorCount = 1,
          .stageFlags = vk::ShaderStageFlagBits::eVertex,
      },
      {
          .binding = 1,
          .descriptorType = vk::DescriptorType::eCombinedImageSampler,
          .descriptorCount = MAX_TEXTURES,
          .stageFlags = vk::ShaderStageFlagBits::eFragment,
      },
  }};

  std::array<vk::DescriptorBindingFlags, 2> flags = {{
      {},
      vk::DescriptorBindingFlagBits::ePartiallyBound,
  }};

  vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
      .bindingCount = static_cast<uint32_t>(flags.size()),
      .pBindingFlags = flags.data(),
  };

  vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .pNext = &flagsInfo,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  return vk::raii::DescriptorSetLayout(gpuContext.getDevice(), layoutInfo);
}

vk::raii::DescriptorSet MainPipeline::createDescriptorSet()
{
  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = gpuResource.getDescriptorPool(),
      .descriptorSetCount = 1,
      .pSetLayouts = &*descriptorSetLayout,
  };

  auto descriptorSet = std::move(gpuContext.getDevice().allocateDescriptorSets(allocInfo).front());

  vk::DescriptorBufferInfo bufferInfo{
      .buffer = gpuUbo.getUniformBuffer(),
      .offset = 0,
      .range = sizeof(UniformBufferObject),
  };

  std::array<vk::WriteDescriptorSet, 1> descriptorWrite{{
      {
          .dstSet = descriptorSet,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eUniformBufferDynamic,
          .pBufferInfo = &bufferInfo,
      },
  }};

  gpuContext.getDevice().updateDescriptorSets(descriptorWrite, {});

  return descriptorSet;
}
