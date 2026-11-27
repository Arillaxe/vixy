#include <renderer/vk/vk_ubo.hpp>

#include <renderer/ubo.hpp>
#include <cassert>
#include <chrono>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

VkUbo::VkUbo(VkContext &context, VkResource &resource, VkSwapchain &swapchain)
    : vkContext(context),
      vkResource(resource),
      vkSwapchain(swapchain),
      frameStride(computeFrameStride()),
      uniformBuffer(vkResource.createBuffer(frameStride * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eUniformBuffer)),
      uniformBufferMemory(vkResource.getBufferMemory(uniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)),
      uniformBufferMapped(uniformBufferMemory.mapMemory(0, frameStride * MAX_FRAMES_IN_FLIGHT)) {}

vk::DeviceSize VkUbo::computeFrameStride()
{
  vk::DeviceSize alignment = vkContext.getPhysicalDevice().getProperties().limits.minUniformBufferOffsetAlignment;
  vk::DeviceSize size = sizeof(UniformBufferObject);

  return (size + alignment - 1) / alignment * alignment;
}

void VkUbo::updateUniformBuffer(Camera &camera, uint32_t frameIndex)
{
  assert(frameIndex < MAX_FRAMES_IN_FLIGHT);

  static auto startTime = std::chrono::high_resolution_clock::now();
  static auto lastTime = startTime;

  auto currentTime = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
  float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();

  lastTime = currentTime;

  UniformBufferObject ubo{};

  ubo.view =
      glm::mat4_cast(glm::conjugate(camera.rotation)) *
      glm::translate(glm::mat4(1.0f), -camera.position);
  ubo.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(vkSwapchain.getExtent().width) / static_cast<float>(vkSwapchain.getExtent().height), 0.1f, 100000.0f);
  ubo.projection[1][1] *= -1;
  ubo.cameraPosition = camera.position;
  ubo.deltaTime = deltaTime * 1000;

  memcpy(static_cast<char *>(uniformBufferMapped) + frameStride * frameIndex, &ubo, sizeof(ubo));
}
