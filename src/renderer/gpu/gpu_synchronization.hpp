#ifndef GPU_SYNCHRONIZATION_HPP
#define GPU_SYNCHRONIZATION_HPP

#include <renderer/gpu/gpu_context.hpp>

class GpuSynchronization
{
private:
  GpuContext &gpuContext;
  vk::raii::Fence drawFence;
  vk::raii::Semaphore presentCompleteSemaphore;

  vk::raii::Fence createFence();
  vk::raii::Semaphore createSemaphore();

public:
  GpuSynchronization(GpuContext &context);

  void waitDrawFence();
  void resetDrawFence();

  vk::raii::Fence &getDrawFence() { return drawFence; }
  vk::raii::Semaphore &getPresentCompleteSemaphore() { return presentCompleteSemaphore; }
};

#endif // GPU_SYNCHRONIZATION_HPP
