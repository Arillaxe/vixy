#include <renderer/gpu/gpu_pipeline.hpp>

#include <stdexcept>

GpuPipeline::GpuPipeline(GpuContext &context, GpuSwapchain &swapchain, GpuResource &resource, const std::string &shaderFilename)
    : gpuContext(context),
      gpuSwapchain(swapchain),
      gpuResource(resource),
      shader(context, shaderFilename),
      gpuUbo(gpuContext, gpuResource, gpuSwapchain) {}

void GpuPipeline::initialize()
{
  descriptorSetLayout = createDescriptorSetLayout();
  pipelineLayout = createPipelineLayout();
  pipeline = createPipeline();
  descriptorSet = createDescriptorSet();
}

vk::Format GpuPipeline::findDepthFormat()
{
  return findSupportedFormat(
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format GpuPipeline::findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
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
