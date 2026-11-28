#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <core/window.hpp>
#include <renderer/vk/vk_context.hpp>
#include <renderer/vk/vk_command.hpp>
#include <renderer/vk/vk_resource.hpp>
#include <renderer/vk/vk_synchronization.hpp>
#include <renderer/vk/vk_swapchain.hpp>
#include <renderer/pipelines/main_pipeline.hpp>
#include <renderer/pipelines/cull_pipeline.hpp>
#include <core/scene.hpp>
#include <core/camera.hpp>
#include <core/thread_queue.hpp>
#include <core/chunk_mesh.hpp>
#include <unordered_map>
#include <renderer/gui.hpp>
#include <renderer/mesh_allocator.hpp>
#include <renderer/texture_manager.hpp>

struct ChunkDrawEntry
{
  MeshAllocation allocation;
  uint32_t slot;
};

class Renderer
{
private:
  Window &window;
  VkContext vkContext;
  VkCommand vkCommand;
  VkResource vkResource;
  std::vector<VkSynchronization> vkSynchronization;
  VkSwapchain vkSwapchain;
  Scene &scene;
  vk::raii::CommandBuffers commandBuffers;
  MainPipeline mainPipeline;
  CullPipeline cullPipeline;
  Camera &camera;
  vk::raii::QueryPool queryPool;
  std::unordered_map<glm::ivec3, ChunkDrawEntry> chunkMeshesAllocations;
  std::vector<uint32_t> freeSlots;
  uint32_t slotCount = 0;
  std::unordered_map<uint32_t, ChunkCullData> dirtySlots;
  ThreadQueue<ChunkMeshMessage> &meshQueue;
  ThreadQueue<MeshAllocation> &freeQueue;
  std::array<std::vector<MeshAllocation>, MAX_FRAMES_IN_FLIGHT> pendingFrees;
  std::vector<MeshAllocation> unsubmittedFrees;
  GUI gui;
  uint32_t meshQueueSize;
  uint32_t totalIndices = 0;
  double cpuMs;
  double gpuMs;
  uint32_t frameIndex = 0;
  bool wireframeMode = true;
  vk::raii::Buffer vertexBuffer;
  vk::raii::DeviceMemory vertexBufferMemory;
  vk::raii::Buffer indexBuffer;
  vk::raii::DeviceMemory indexBufferMemory;
  vk::raii::Buffer indirectBuffer;
  vk::raii::DeviceMemory indirectBufferMemory;
  vk::raii::Buffer instanceBuffer;
  vk::raii::DeviceMemory instanceBufferMemory;
  vk::raii::Buffer drawCountBuffer;
  vk::raii::DeviceMemory drawCountBufferMemory;
  vk::raii::Buffer chunkTableBuffer;
  vk::raii::DeviceMemory chunkTableBufferMemory;
  vk::raii::Buffer chunkStagingBuffer;
  vk::raii::DeviceMemory chunkStagingBufferMemory;
  ChunkCullData *chunkStaging = nullptr;
  TextureManager textureManager;
  vk::raii::Sampler sampler;

  void transition_image_layout(
      vk::raii::CommandBuffer &commandBuffer,
      vk::Image image,
      vk::ImageLayout old_layout,
      vk::ImageLayout new_layout,
      vk::AccessFlags2 src_access_mask,
      vk::AccessFlags2 dst_access_mask,
      vk::PipelineStageFlags2 src_stage_mask,
      vk::PipelineStageFlags2 dst_stage_mask,
      vk::ImageAspectFlags image_aspect_flags);
  void applyMeshMessages();
  void recordCulling(vk::raii::CommandBuffer &commandBuffer);
  void memoryBarrier(
      vk::raii::CommandBuffer &commandBuffer,
      vk::PipelineStageFlags2 src_stage_mask,
      vk::AccessFlags2 src_access_mask,
      vk::PipelineStageFlags2 dst_stage_mask,
      vk::AccessFlags2 dst_access_mask);

public:
  Renderer(Window &win, Scene &_scene, Camera &cam, ThreadQueue<ChunkMeshMessage> &mQueue, ThreadQueue<MeshAllocation> &fQueue);

  VkResource &getVkResource() { return vkResource; }
  std::pair<vk::raii::Buffer &, vk::raii::Buffer &> getBuffers() { return {vertexBuffer, indexBuffer}; }
  vk::raii::Buffer &getVertexBuffer() { return vertexBuffer; }
  vk::raii::Buffer &getIndexBuffer() { return indexBuffer; }
  vk::raii::Sampler createSampler();

  uint32_t loadTexture(const std::string &filename);
  void render();
  void waitIdle();
  void drawGUI();
  void toggleWireframeMode() { wireframeMode = !wireframeMode; }
};

#endif // RENDERER_HPP
