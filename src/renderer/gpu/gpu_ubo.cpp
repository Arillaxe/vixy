#include <renderer/gpu/gpu_ubo.hpp>

#include <renderer/ubo.hpp>
#include <cassert>
#include <chrono>
#include <cstring>

GpuUbo::GpuUbo(GpuContext &context, GpuResource &resource, GpuSwapchain &swapchain)
    : gpuContext(context),
      gpuResource(resource),
      gpuSwapchain(swapchain),
      frameStride(computeFrameStride()),
      uniformBuffer(gpuResource.createBuffer(frameStride * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eUniformBuffer)),
      uniformBufferMemory(gpuResource.getBufferMemory(uniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)),
      uniformBufferMapped(uniformBufferMemory.mapMemory(0, frameStride * MAX_FRAMES_IN_FLIGHT)) {}

vk::DeviceSize GpuUbo::computeFrameStride()
{
  vk::DeviceSize alignment = gpuContext.getPhysicalDevice().getProperties().limits.minUniformBufferOffsetAlignment;
  vk::DeviceSize size = sizeof(UniformBufferObject);

  return (size + alignment - 1) / alignment * alignment;
}

void GpuUbo::updateUniformBuffer(Camera &camera, uint32_t frameIndex)
{
  assert(frameIndex < MAX_FRAMES_IN_FLIGHT);

  static auto startTime = std::chrono::high_resolution_clock::now();
  static auto lastTime = startTime;

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
  float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();

  lastTime = currentTime;

  UniformBufferObject ubo{};

  ubo.view = camera.viewMatrix();
  ubo.projection = camera.projectionMatrix(static_cast<float>(gpuSwapchain.getExtent().width) / static_cast<float>(gpuSwapchain.getExtent().height));
  ubo.projection[1][1] *= -1;
  ubo.cameraPosition = camera.position;
  ubo.deltaTime = deltaTime * 1000;

  memcpy(static_cast<char *>(uniformBufferMapped) + frameStride * frameIndex, &ubo, sizeof(ubo));
}
