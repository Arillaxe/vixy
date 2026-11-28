#ifndef GPU_RESOURCE_HPP
#define GPU_RESOURCE_HPP

#include <renderer/vk.hpp>
#include <renderer/gpu/gpu_context.hpp>
#include <renderer/gpu/gpu_command.hpp>

class GpuResource
{
private:
  GpuContext &gpuContext;
  GpuCommand &gpuCommand;
  vk::raii::DescriptorPool descriptorPool;

  uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
  vk::raii::DescriptorPool createDescriptorPool();
  void transitionImageLayout(
      vk::raii::CommandBuffer &commandBuffer,
      vk::Image image,
      vk::ImageLayout oldLayout,
      vk::ImageLayout newLayout,
      vk::AccessFlags2 srcAccessMask,
      vk::AccessFlags2 dstAccessMask,
      vk::PipelineStageFlags2 srcStageMask,
      vk::PipelineStageFlags2 dstStageMask,
      vk::ImageAspectFlags imageAspectFlags);

public:
  GpuResource(GpuContext &context, GpuCommand &command);

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
  GpuContext &getContext() { return gpuContext; }
};

#endif // GPU_RESOURCE_HPP
