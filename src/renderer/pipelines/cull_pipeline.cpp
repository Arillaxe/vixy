#include <renderer/pipelines/cull_pipeline.hpp>

#include <array>

CullPipeline::CullPipeline(VkContext &context, VkResource &resource)
    : vkContext(context),
      vkResource(resource),
      shader(context, "cull.spv"),
      descriptorSetLayout(createDescriptorSetLayout()),
      pipelineLayout(createPipelineLayout()),
      pipeline(createPipeline()),
      descriptorSet(createDescriptorSet()) {}

vk::raii::DescriptorSetLayout CullPipeline::createDescriptorSetLayout()
{
  std::array<vk::DescriptorSetLayoutBinding, 4> bindings{};

  for (uint32_t i = 0; i < bindings.size(); i++)
  {
    bindings[i] = vk::DescriptorSetLayoutBinding{
        .binding = i,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eCompute,
    };
  }

  vk::DescriptorSetLayoutCreateInfo layoutInfo{
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  return vk::raii::DescriptorSetLayout(vkContext.getDevice(), layoutInfo);
}

vk::raii::PipelineLayout CullPipeline::createPipelineLayout()
{
  vk::PushConstantRange pushRange{
      .stageFlags = vk::ShaderStageFlagBits::eCompute,
      .offset = 0,
      .size = sizeof(CullPushConstants),
  };

  vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
      .setLayoutCount = 1,
      .pSetLayouts = &*descriptorSetLayout,
      .pushConstantRangeCount = 1,
      .pPushConstantRanges = &pushRange,
  };

  return vk::raii::PipelineLayout(vkContext.getDevice(), pipelineLayoutInfo);
}

vk::raii::Pipeline CullPipeline::createPipeline()
{
  vk::ComputePipelineCreateInfo pipelineInfo{
      .stage = {
          .stage = vk::ShaderStageFlagBits::eCompute,
          .module = shader.getShaderModule(),
          .pName = "cullMain",
      },
      .layout = pipelineLayout,
  };

  return vk::raii::Pipeline(vkContext.getDevice(), nullptr, pipelineInfo);
}

vk::raii::DescriptorSet CullPipeline::createDescriptorSet()
{
  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = vkResource.getDescriptorPool(),
      .descriptorSetCount = 1,
      .pSetLayouts = &*descriptorSetLayout,
  };

  return std::move(vkContext.getDevice().allocateDescriptorSets(allocInfo).front());
}

void CullPipeline::bindBuffers(vk::Buffer chunks, vk::Buffer commands, vk::Buffer instances, vk::Buffer counts)
{
  std::array<vk::Buffer, 4> buffers{chunks, commands, instances, counts};
  std::array<vk::DescriptorBufferInfo, 4> bufferInfos{};
  std::array<vk::WriteDescriptorSet, 4> writes{};

  for (uint32_t i = 0; i < buffers.size(); i++)
  {
    bufferInfos[i] = vk::DescriptorBufferInfo{
        .buffer = buffers[i],
        .offset = 0,
        .range = vk::WholeSize,
    };

    writes[i] = vk::WriteDescriptorSet{
        .dstSet = *descriptorSet,
        .dstBinding = i,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eStorageBuffer,
        .pBufferInfo = &bufferInfos[i],
    };
  }

  vkContext.getDevice().updateDescriptorSets(writes, {});
}

void CullPipeline::dispatch(vk::raii::CommandBuffer &commandBuffer, const CullPushConstants &pushConstants)
{
  if (pushConstants.slotCount == 0)
    return;

  commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
  commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *pipelineLayout, 0, *descriptorSet, {});
  commandBuffer.pushConstants<CullPushConstants>(*pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pushConstants);
  commandBuffer.dispatch((pushConstants.slotCount + CULL_WORKGROUP_SIZE - 1) / CULL_WORKGROUP_SIZE, 1, 1);
}
