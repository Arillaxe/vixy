#include <renderer/gpu/gpu_synchronization.hpp>

#include <stdexcept>

GpuSynchronization::GpuSynchronization(GpuContext &context)
    : gpuContext(context),
      drawFence(createFence()),
      presentCompleteSemaphore(createSemaphore()) {}

vk::raii::Fence GpuSynchronization::createFence()
{
  vk::FenceCreateInfo fenceInfo{
      .flags = vk::FenceCreateFlagBits::eSignaled,
  };

  return vk::raii::Fence(gpuContext.getDevice(), fenceInfo);
}

vk::raii::Semaphore GpuSynchronization::createSemaphore()
{
  return vk::raii::Semaphore(gpuContext.getDevice(), vk::SemaphoreCreateInfo());
}

void GpuSynchronization::waitDrawFence()
{
  auto fenceResult = gpuContext.getDevice().waitForFences(*drawFence, vk::True, UINT64_MAX);

  if (fenceResult != vk::Result::eSuccess)
  {
    throw std::runtime_error("failed to wait for fence!");
  }
}

void GpuSynchronization::resetDrawFence()
{
  gpuContext.getDevice().resetFences(*drawFence);
}
