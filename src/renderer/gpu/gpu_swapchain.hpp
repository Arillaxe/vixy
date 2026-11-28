#ifndef GPU_SWAPCHAIN_HPP
#define GPU_SWAPCHAIN_HPP

#include <renderer/vk.hpp>
#include <core/window.hpp>
#include <renderer/gpu/gpu_context.hpp>
#include <renderer/gpu/gpu_resource.hpp>
#include <renderer/gpu/gpu_synchronization.hpp>
#include <vector>

class GpuSwapchain
{
private:
  Window &window;
  GpuContext &gpuContext;
  GpuResource &gpuResource;
  std::vector<GpuSynchronization> &gpuSynchronization;
  vk::SurfaceCapabilitiesKHR capabilities;
  vk::Extent2D extent;
  vk::SurfaceFormatKHR surfaceFormat;
  vk::raii::SwapchainKHR swapchain;
  std::vector<vk::Image> images;
  std::vector<vk::raii::ImageView> imageViews;
  // One per swapchain image: present may hold the semaphore until that image is re-acquired.
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;

  vk::raii::Image depthImage;
  vk::raii::DeviceMemory depthImageMemory;
  vk::raii::ImageView depthImageView;

  vk::raii::Image colorImage;
  vk::raii::DeviceMemory colorImageMemory;
  vk::raii::ImageView colorImageView;

  vk::SurfaceCapabilitiesKHR getCapabilities();
  vk::Extent2D getSwapExtent();
  vk::SurfaceFormatKHR findSurfaceFormat();
  vk::PresentModeKHR getPresentMode();
  vk::raii::SwapchainKHR createSwapchain(const vk::SwapchainKHR &oldSwapchain);
  std::vector<vk::raii::ImageView> createImageViews();
  std::vector<vk::raii::Semaphore> createRenderFinishedSemaphores();
  vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

public:
  GpuSwapchain(GpuContext &context, Window &win, GpuResource &resource, std::vector<GpuSynchronization> &synchronization);

  void recreateSwapchain();
  uint32_t acquireNextImage(uint32_t frameIndex);

  vk::Format findDepthFormat();
  vk::SurfaceFormatKHR &getSurfaceFormat() { return surfaceFormat; }
  vk::raii::SwapchainKHR &getSwapchain() { return swapchain; }
  vk::Extent2D &getExtent() { return extent; }
  std::vector<vk::Image> &getImages() { return images; }
  std::vector<vk::raii::ImageView> &getImageViews() { return imageViews; }
  vk::raii::Semaphore &getRenderFinishedSemaphore(uint32_t imageIndex) { return renderFinishedSemaphores[imageIndex]; }
  vk::raii::Image &getDepthImage() { return depthImage; }
  vk::raii::ImageView &getColorImageView() { return colorImageView; }
  vk::raii::Image &getColorImage() { return colorImage; }
  vk::raii::ImageView &getDepthImageView() { return depthImageView; }
};

#endif // GPU_SWAPCHAIN_HPP
