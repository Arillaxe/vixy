#ifndef VK_UBO_HPP
#define VK_UBO_HPP

#include <renderer/vk/vk_context.hpp>
#include <renderer/vk/vk_resource.hpp>
#include <renderer/vk/vk_swapchain.hpp>
#include <core/camera.hpp>

class VkUbo
{
private:
  VkContext &vkContext;
  VkResource &vkResource;
  VkSwapchain &vkSwapchain;
  vk::DeviceSize frameStride;
  vk::raii::Buffer uniformBuffer;
  vk::raii::DeviceMemory uniformBufferMemory;
  void *uniformBufferMapped;

  vk::DeviceSize computeFrameStride();

public:
  VkUbo(VkContext &context, VkResource &resource, VkSwapchain &swapchain);

  vk::raii::Buffer &getUniformBuffer() { return uniformBuffer; }
  uint32_t getDynamicOffset(uint32_t frameIndex) const { return static_cast<uint32_t>(frameStride * frameIndex); }

  void updateUniformBuffer(Camera &camera, uint32_t frameIndex);
};

#endif // VK_UBO_HPP
