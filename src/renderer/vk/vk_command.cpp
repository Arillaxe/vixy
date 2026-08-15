#include <renderer/vk/vk_command.hpp>

VkCommand::VkCommand(VkContext &context)
    : vkContext(context),
      graphicsCommandPool(createCommandPool(context.getQueueFamilyIndex())),
      transferCommandPool(createCommandPool(context.getTransferQueueFamilyIndex())) {}

vk::raii::CommandPool VkCommand::createCommandPool(uint32_t queueFamilyIndex)
{
  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndex,
  };

  return vk::raii::CommandPool(vkContext.getDevice(), poolInfo);
}

vk::raii::CommandBuffers VkCommand::createCommandBuffers(uint32_t amount)
{
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = graphicsCommandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = amount,
  };

  return vk::raii::CommandBuffers(vkContext.getDevice(), allocInfo);
}

vk::raii::CommandBuffer VkCommand::beginSingleTimeCommands(bool q)
{
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = q ? graphicsCommandPool : transferCommandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(vkContext.getDevice(), allocInfo).front());
  vk::CommandBufferBeginInfo beginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  };
  commandBuffer.begin(beginInfo);

  return std::move(commandBuffer);
}

void VkCommand::endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer, bool q)
{
  commandBuffer.end();

  vk::SubmitInfo submitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffer,
  };

  if (q)
  {
    vkContext.getQueue().submit(submitInfo, nullptr);
    vkContext.getQueue().waitIdle();
  }
  else
  {
    vkContext.getTransferQueue().submit(submitInfo, nullptr);
    vkContext.getTransferQueue().waitIdle();
  }
}
