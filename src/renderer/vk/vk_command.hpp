#ifndef VK_COMMAND_HPP
#define VK_COMMAND_HPP

#include <renderer/vk/vk_context.hpp>
#include <functional>

class VkCommand
{
private:
  VkContext &vkContext;
  vk::raii::CommandPool graphicsCommandPool;
  vk::raii::CommandPool singleTimeGraphicsCommandPool;
  vk::raii::CommandPool transferCommandPool;

  vk::raii::CommandPool createCommandPool(uint32_t queueFamilyIndex);

public:
  VkCommand(VkContext &context);

  vk::raii::CommandBuffers createCommandBuffers(uint32_t amount);
  void submitSingleTimeCommands(const std::function<void(vk::raii::CommandBuffer &)> &record, bool graphics = false);
};

#endif // VK_COMMAND_HPP
