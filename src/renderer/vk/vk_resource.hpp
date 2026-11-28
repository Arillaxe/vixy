#ifndef VK_RESOURCE_HPP
#define VK_RESOURCE_HPP

#include <renderer/vk.hpp>
#include <renderer/vk/vk_context.hpp>
#include <renderer/vk/vk_command.hpp>

class VkResource
{
private:
  VkContext &vkContext;
  VkCommand &vkCommand;
  vk::raii::DescriptorPool descriptorPool;

  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
  vk::raii::DescriptorPool createDescriptorPool();
  void transition_image_layout(
      vk::raii::CommandBuffer &commandBuffer,
      vk::Image image,
      vk::ImageLayout old_layout,
      vk::ImageLayout new_layout,
      vk::AccessFlags2 src_access_mask,
      vk::AccessFlags2 dst_access_mask,
      vk::PipelineStageFlags2 src_stage_mask,
      vk::PipelineStageFlags2 dst_stage_mask,
      vk::ImageAspectFlags image_aspect_flags);

public:
  VkResource(VkContext &context, VkCommand &command);

  vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlagBits aspectFlags, uint32_t mipLevels) const;
  vk::raii::Buffer createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, bool shared = false);
  vk::raii::DeviceMemory getBufferMemory(vk::raii::Buffer &buffer, vk::MemoryPropertyFlags properties);
  vk::raii::DeviceMemory getImageMemory(vk::raii::Image &image, vk::MemoryPropertyFlags properties);
  void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size);
  vk::raii::Image createImage(
      uint32_t width,
      uint32_t height,
      uint32_t mipLevels,
      vk::SampleCountFlagBits numSamples,
      vk::Format format,
      vk::ImageTiling tiling,
      vk::ImageUsageFlags usage);
  void copyBufferEx(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::BufferCopy copyParams);
  void uploadImage(vk::raii::Buffer &stagingBuffer, vk::raii::Image &image, int width, int height);

  vk::raii::DescriptorPool &getDescriptorPool() { return descriptorPool; }
  VkContext &getContext() { return vkContext; }
};

#endif // VK_RESOURCE_HPP
