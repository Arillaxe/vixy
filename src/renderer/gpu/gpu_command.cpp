#include <renderer/gpu/gpu_command.hpp>

GpuCommand::GpuCommand(GpuContext &context)
    : gpuContext(context),
      graphicsCommandPool(createCommandPool(context.getQueueFamilyIndex())),
      singleTimeGraphicsCommandPool(createCommandPool(context.getQueueFamilyIndex())),
      transferCommandPool(createCommandPool(context.getTransferQueueFamilyIndex())) {}

vk::raii::CommandPool GpuCommand::createCommandPool(uint32_t queueFamilyIndex)
{
  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = queueFamilyIndex,
  };

  return vk::raii::CommandPool(gpuContext.getDevice(), poolInfo);
}

vk::raii::CommandBuffers GpuCommand::createCommandBuffers(uint32_t amount)
{
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = graphicsCommandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = amount,
  };

  return vk::raii::CommandBuffers(gpuContext.getDevice(), allocInfo);
}

void GpuCommand::submitSingleTimeCommands(const std::function<void(vk::raii::CommandBuffer &)> &record, bool graphics)
{
  vk::raii::Queue &queue = graphics ? gpuContext.getQueue() : gpuContext.getTransferQueue();
  std::lock_guard lock(graphics ? gpuContext.getQueueMutex() : gpuContext.getTransferQueueMutex());

  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = graphics ? singleTimeGraphicsCommandPool : transferCommandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  vk::raii::CommandBuffer commandBuffer = std::move(vk::raii::CommandBuffers(gpuContext.getDevice(), allocInfo).front());

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
