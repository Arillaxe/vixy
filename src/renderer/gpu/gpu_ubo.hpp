#ifndef GPU_UBO_HPP
#define GPU_UBO_HPP

#include <renderer/gpu/gpu_context.hpp>
#include <renderer/gpu/gpu_resource.hpp>
#include <renderer/gpu/gpu_swapchain.hpp>
#include <core/camera.hpp>

class GpuUbo
{
private:
  GpuContext &gpuContext;
  GpuResource &gpuResource;
  GpuSwapchain &gpuSwapchain;
  vk::DeviceSize frameStride;
  vk::raii::Buffer uniformBuffer;
  vk::raii::DeviceMemory uniformBufferMemory;
  void *uniformBufferMapped;

  vk::DeviceSize computeFrameStride();

public:
  GpuUbo(GpuContext &context, GpuResource &resource, GpuSwapchain &swapchain);

  vk::raii::Buffer &getUniformBuffer() { return uniformBuffer; }
  uint32_t getDynamicOffset(uint32_t frameIndex) const { return static_cast<uint32_t>(frameStride * frameIndex); }

  void updateUniformBuffer(Camera &camera, uint32_t frameIndex);
};

#endif // GPU_UBO_HPP
