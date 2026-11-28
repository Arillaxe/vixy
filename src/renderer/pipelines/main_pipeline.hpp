#ifndef MAIN_PIPELINE_HPP
#define MAIN_PIPELINE_HPP

#include <renderer/gpu/gpu_pipeline.hpp>

class MainPipeline : public GpuPipeline
{
private:
  vk::raii::Pipeline entityPipeline = nullptr;

  vk::raii::Pipeline buildPipeline(const vk::PipelineVertexInputStateCreateInfo &vertexInputInfo, const char *vertexEntry);
  vk::raii::Pipeline createEntityPipeline();

protected:
  vk::raii::PipelineLayout createPipelineLayout() override;
  vk::raii::Pipeline createPipeline() override;
  vk::raii::DescriptorSetLayout createDescriptorSetLayout() override;
  vk::raii::DescriptorSet createDescriptorSet() override;

public:
  MainPipeline(GpuContext &context, GpuSwapchain &swapchain, GpuResource &resource);

  vk::raii::Pipeline &getEntityPipeline() { return entityPipeline; }
};

#endif // MAIN_PIPELINE_HPP
