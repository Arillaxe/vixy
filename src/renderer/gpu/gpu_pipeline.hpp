#ifndef GPU_PIPELINE_HPP
#define GPU_PIPELINE_HPP

#include <renderer/vk.hpp>
#include <renderer/gpu/gpu_context.hpp>
#include <renderer/gpu/gpu_shader.hpp>
#include <renderer/gpu/gpu_swapchain.hpp>
#include <renderer/gpu/gpu_ubo.hpp>
#include <renderer/vertex.hpp>
#include <renderer/push.hpp>
#include <array>
#include <string>
#include <vector>

class GpuPipeline
{
protected:
  GpuContext &gpuContext;
  GpuSwapchain &gpuSwapchain;
  GpuResource &gpuResource;
  GpuShader shader;
  vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  vk::raii::Pipeline pipeline = nullptr;
  vk::raii::DescriptorSet descriptorSet = nullptr;
  GpuUbo gpuUbo;

  void initialize();

  virtual vk::raii::PipelineLayout createPipelineLayout() = 0;
  virtual vk::raii::Pipeline createPipeline() = 0;
  virtual vk::raii::DescriptorSetLayout createDescriptorSetLayout() = 0;
  virtual vk::raii::DescriptorSet createDescriptorSet() = 0;

  vk::Format findDepthFormat();
  vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

public:
  GpuPipeline(GpuContext &context, GpuSwapchain &swapchain, GpuResource &resource, const std::string &shaderFilename);
  virtual ~GpuPipeline() = default;

  GpuUbo &getGpuUbo() { return gpuUbo; }
  vk::raii::Pipeline &getPipeline() { return pipeline; }
  vk::raii::PipelineLayout &getPipelineLayout() { return pipelineLayout; }
  vk::raii::DescriptorSet &getDescriptorSet() { return descriptorSet; }
};

#endif // GPU_PIPELINE_HPP
