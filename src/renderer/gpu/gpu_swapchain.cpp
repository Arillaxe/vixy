#include <renderer/gpu/gpu_swapchain.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>

GpuSwapchain::GpuSwapchain(GpuContext &context, Window &win, GpuResource &resource, std::vector<GpuSynchronization> &synchronization)
    : window(win),
      gpuContext(context),
      gpuResource(resource),
      gpuSynchronization(synchronization),
      capabilities(getCapabilities()),
      extent(getSwapExtent()),
      surfaceFormat(findSurfaceFormat()),
      swapchain(createSwapchain(VK_NULL_HANDLE)),
      images(swapchain.getImages()),
      imageViews(createImageViews()),
      renderFinishedSemaphores(createRenderFinishedSemaphores()),
      depthImage(resource.createImage(extent.width, extent.height, 1, gpuContext.getMsaaSamples(), findDepthFormat(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment)),
      depthImageMemory(resource.getImageMemory(depthImage, vk::MemoryPropertyFlagBits::eDeviceLocal)),
      depthImageView(resource.createImageView(depthImage, findDepthFormat(), vk::ImageAspectFlagBits::eDepth, 1)),
      colorImage(resource.createImage(extent.width, extent.height, 1, gpuContext.getMsaaSamples(), surfaceFormat.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment)),
      colorImageMemory(resource.getImageMemory(colorImage, vk::MemoryPropertyFlagBits::eDeviceLocal)),
      colorImageView(resource.createImageView(colorImage, surfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1)) {}

vk::SurfaceCapabilitiesKHR GpuSwapchain::getCapabilities()
{
  return gpuContext
      .getPhysicalDevice()
      .getSurfaceCapabilitiesKHR(gpuContext.getSurface());
}

vk::Extent2D GpuSwapchain::getSwapExtent()
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }

  int width, height;
  glfwGetFramebufferSize(window.getGLFWWindowHandle(), &width, &height);

  return {
      std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
      std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
  };
}

vk::SurfaceFormatKHR GpuSwapchain::findSurfaceFormat()
{
  std::vector<vk::SurfaceFormatKHR> formats =
      gpuContext
          .getPhysicalDevice()
          .getSurfaceFormatsKHR(gpuContext.getSurface());

  const auto formatIt = std::ranges::find_if(
      formats, [](const auto &format)
      { return format.format == vk::Format::eB8G8R8A8Srgb &&
               format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });

  return formatIt != formats.end() ? *formatIt : formats[0];
}

vk::PresentModeKHR GpuSwapchain::getPresentMode()
{
  std::vector<vk::PresentModeKHR> presentModes =
      gpuContext
          .getPhysicalDevice()
          .getSurfacePresentModesKHR(gpuContext.getSurface());

  return std::ranges::any_of(
             presentModes, [](const auto mode)
             { return mode == vk::PresentModeKHR::eMailbox; })
             ? vk::PresentModeKHR::eMailbox
             : vk::PresentModeKHR::eFifo;
}

vk::raii::SwapchainKHR GpuSwapchain::createSwapchain(const vk::SwapchainKHR &oldSwapchain)
{
  auto minImageCount = std::max(3u, capabilities.minImageCount);

  if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < minImageCount)
  {
    minImageCount = capabilities.maxImageCount;
  }

  vk::SwapchainCreateInfoKHR swapChainCreateInfo{
      .surface = gpuContext.getSurface(),
      .minImageCount = minImageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .preTransform = capabilities.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = getPresentMode(),
      .clipped = true,
  };

  swapChainCreateInfo.oldSwapchain = oldSwapchain;

  return vk::raii::SwapchainKHR(gpuContext.getDevice(), swapChainCreateInfo);
}

std::vector<vk::raii::ImageView> GpuSwapchain::createImageViews()
{
  std::vector<vk::raii::ImageView> views;

  views.reserve(images.size());

  for (auto &image : images)
  {
    views.emplace_back(
        gpuResource.createImageView(
            image,
            surfaceFormat.format,
            vk::ImageAspectFlagBits::eColor,
            1));
  }

  return views;
}

std::vector<vk::raii::Semaphore> GpuSwapchain::createRenderFinishedSemaphores()
{
  std::vector<vk::raii::Semaphore> semaphores;
  semaphores.reserve(images.size());

  for (size_t i = 0; i < images.size(); ++i)
  {
    semaphores.emplace_back(gpuContext.getDevice(), vk::SemaphoreCreateInfo{});
  }

  return semaphores;
}

vk::Format GpuSwapchain::findDepthFormat()
{
  return findSupportedFormat(
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format GpuSwapchain::findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
  for (const auto format : candidates)
  {
    vk::FormatProperties props = gpuContext.getPhysicalDevice().getFormatProperties(format);

    if (
        (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) ||
        (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features))
    {
      return format;
    }
  }

  throw std::runtime_error("failed to find supported format!");
}

void GpuSwapchain::recreateSwapchain()
{
  int width = 0;
  int height = 0;

  glfwGetFramebufferSize(window.getGLFWWindowHandle(), &width, &height);

  while (width == 0 || height == 0)
  {
    glfwGetFramebufferSize(window.getGLFWWindowHandle(), &width, &height);
    glfwWaitEvents();
  }

  gpuContext.waitIdle();

  auto oldSwapchain = std::move(swapchain);

  capabilities = getCapabilities();
  surfaceFormat = findSurfaceFormat();
  extent = getSwapExtent();

  swapchain = createSwapchain(oldSwapchain);
  images = swapchain.getImages();
  imageViews.clear();
  imageViews = createImageViews();
  renderFinishedSemaphores.clear();
  renderFinishedSemaphores = createRenderFinishedSemaphores();

  depthImage = gpuResource.createImage(extent.width, extent.height, 1, gpuContext.getMsaaSamples(), findDepthFormat(), vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment);
  depthImageMemory = gpuResource.getImageMemory(depthImage, vk::MemoryPropertyFlagBits::eDeviceLocal);
  depthImageView = gpuResource.createImageView(depthImage, findDepthFormat(), vk::ImageAspectFlagBits::eDepth, 1);
  colorImage = gpuResource.createImage(extent.width, extent.height, 1, gpuContext.getMsaaSamples(), surfaceFormat.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment);
  colorImageMemory = gpuResource.getImageMemory(colorImage, vk::MemoryPropertyFlagBits::eDeviceLocal);
  colorImageView = gpuResource.createImageView(colorImage, surfaceFormat.format, vk::ImageAspectFlagBits::eColor, 1);
}

uint32_t GpuSwapchain::acquireNextImage(uint32_t frameIndex)
{
  auto [result, imageIndex] = swapchain.acquireNextImage(UINT64_MAX, *gpuSynchronization[frameIndex].getPresentCompleteSemaphore(), nullptr);

  if (result == vk::Result::eErrorOutOfDateKHR)
  {
    recreateSwapchain();

    return acquireNextImage(frameIndex);
  }

  if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
  {
    assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);

    throw std::runtime_error("failed to aquire swap chain image!");
  }

  return imageIndex;
}
