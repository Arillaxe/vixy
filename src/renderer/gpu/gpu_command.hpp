#ifndef GPU_COMMAND_HPP
#define GPU_COMMAND_HPP

#include <renderer/gpu/gpu_context.hpp>
#include <functional>

class GpuCommand
{
private:
  GpuContext &gpuContext;
  vk::raii::CommandPool graphicsCommandPool;
  vk::raii::CommandPool singleTimeGraphicsCommandPool;
  vk::raii::CommandPool transferCommandPool;

  vk::raii::CommandPool createCommandPool(uint32_t queueFamilyIndex);

public:
  GpuCommand(GpuContext &context);

  vk::raii::CommandBuffers createCommandBuffers(uint32_t amount);
  void submitSingleTimeCommands(const std::function<void(vk::raii::CommandBuffer &)> &record, bool graphics = false);
};

#endif // GPU_COMMAND_HPP
