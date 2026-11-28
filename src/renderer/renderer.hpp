#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <core/window.hpp>
#include <renderer/gpu/gpu_context.hpp>
#include <renderer/gpu/gpu_command.hpp>
#include <renderer/gpu/gpu_resource.hpp>
#include <renderer/gpu/gpu_synchronization.hpp>
#include <renderer/gpu/gpu_swapchain.hpp>
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
  GpuContext gpuContext;
  GpuCommand gpuCommand;
  GpuResource gpuResource;
  std::vector<GpuSynchronization> gpuSynchronization;
  GpuSwapchain gpuSwapchain;
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
  uint32_t meshQueueSize = 0;
  uint32_t totalIndices = 0;
  double cpuMs = 0.0;
  double gpuMs = 0.0;
  uint32_t frameIndex = 0;
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

  void transitionImageLayout(
      vk::raii::CommandBuffer &commandBuffer,
      vk::Image image,
      vk::ImageLayout oldLayout,
      vk::ImageLayout newLayout,
      vk::AccessFlags2 srcAccessMask,
      vk::AccessFlags2 dstAccessMask,
      vk::PipelineStageFlags2 srcStageMask,
      vk::PipelineStageFlags2 dstStageMask,
      vk::ImageAspectFlags imageAspectFlags);
  void applyMeshMessages();
  void recordCulling(vk::raii::CommandBuffer &commandBuffer);
  void memoryBarrier(
      vk::raii::CommandBuffer &commandBuffer,
      vk::PipelineStageFlags2 srcStageMask,
      vk::AccessFlags2 srcAccessMask,
      vk::PipelineStageFlags2 dstStageMask,
      vk::AccessFlags2 dstAccessMask);

public:
  Renderer(Window &win, Scene &_scene, Camera &cam, ThreadQueue<ChunkMeshMessage> &mQueue, ThreadQueue<MeshAllocation> &fQueue);

  GpuResource &getGpuResource() { return gpuResource; }
  std::pair<vk::raii::Buffer &, vk::raii::Buffer &> getBuffers() { return {vertexBuffer, indexBuffer}; }
  vk::raii::Buffer &getVertexBuffer() { return vertexBuffer; }
  vk::raii::Buffer &getIndexBuffer() { return indexBuffer; }
  vk::raii::Sampler createSampler();

  uint32_t loadTexture(const std::string &filename);
  void render();
  void waitIdle();
  void drawGUI();
};

#endif // RENDERER_HPP
