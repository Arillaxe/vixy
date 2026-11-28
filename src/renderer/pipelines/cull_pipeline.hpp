#ifndef CULL_PIPELINE_HPP
#define CULL_PIPELINE_HPP

#include <renderer/vk.hpp>
#include <renderer/vk/vk_context.hpp>
#include <renderer/vk/vk_resource.hpp>
#include <renderer/vk/vk_shader.hpp>
#include <cstddef>

struct ChunkCullData
{
  glm::vec4 originScale;
  uint32_t indexCount;
  uint32_t firstIndex;
  int32_t vertexOffset;
  uint32_t pad;
};

static_assert(sizeof(ChunkCullData) == 32 && offsetof(ChunkCullData, indexCount) == 16);

struct CullPushConstants
{
  glm::vec4 planes[6];
  uint32_t slotCount;
  uint32_t outputBase;
  uint32_t countIndex;
  uint32_t maxDraws;
  float chunkExtent;
};

static_assert(sizeof(CullPushConstants) == 116 && offsetof(CullPushConstants, slotCount) == 96);

constexpr uint32_t CULL_WORKGROUP_SIZE = 64;

class CullPipeline
{
private:
  VkContext &vkContext;
  VkResource &vkResource;
  VkShader shader;
  vk::raii::DescriptorSetLayout descriptorSetLayout;
  vk::raii::PipelineLayout pipelineLayout;
  vk::raii::Pipeline pipeline;
  vk::raii::DescriptorSet descriptorSet;

  vk::raii::DescriptorSetLayout createDescriptorSetLayout();
  vk::raii::PipelineLayout createPipelineLayout();
  vk::raii::Pipeline createPipeline();
  vk::raii::DescriptorSet createDescriptorSet();

public:
  CullPipeline(VkContext &context, VkResource &resource);

  void bindBuffers(vk::Buffer chunks, vk::Buffer commands, vk::Buffer instances, vk::Buffer counts);
  void dispatch(vk::raii::CommandBuffer &commandBuffer, const CullPushConstants &pushConstants);
};

#endif // CULL_PIPELINE_HPP
