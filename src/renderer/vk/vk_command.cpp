#include <renderer/vk/vk_command.hpp>

VkCommand::VkCommand(VkContext &context)
    : vkContext(context),
      graphicsCommandPool(createCommandPool(context.getQueueFamilyIndex())),
      singleTimeGraphicsCommandPool(createCommandPool(context.getQueueFamilyIndex())),
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

void VkCommand::submitSingleTimeCommands(const std::function<void(vk::raii::CommandBuffer &)> &record, bool graphics)
{
  vk::raii::Queue &queue = graphics ? vkContext.getQueue() : vkContext.getTransferQueue();
  std::lock_guard lock(graphics ? vkContext.getQueueMutex() : vkContext.getTransferQueueMutex());

  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = graphics ? singleTimeGraphicsCommandPool : transferCommandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(vkContext.getDevice(), allocInfo).front());

  vk::CommandBufferBeginInfo beginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  };
  commandBuffer.begin(beginInfo);
  record(commandBuffer);
  commandBuffer.end();

  vk::SubmitInfo submitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffer,
  };
  queue.submit(submitInfo, nullptr);
  queue.waitIdle();
}
