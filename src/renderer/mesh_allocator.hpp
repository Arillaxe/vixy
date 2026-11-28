#ifndef MESH_ALLOCATOR_HPP
#define MESH_ALLOCATOR_HPP

#include <cstdint>
#include <variant>
#include <core/arena_allocator.hpp>
#include <renderer/vertex.hpp>
#include <renderer/vk/vk_resource.hpp>

constexpr uint32_t VERTEX_BUFFER_SIZE = 64 * 1024 * 1024; // 64 MiB
constexpr uint32_t INDEX_BUFFER_SIZE = 32 * 1024 * 1024;  // 32 MiB
constexpr uint32_t STAGING_BUFFER_SIZE = 16 * 1024 * 1024;

struct MeshAllocation
{
  glm::ivec3 pos;
  uint32_t vertexOffset;
  uint32_t vertexCount;
  uint32_t indexOffset;
  uint32_t indexCount;
};

// Messages from the chunk thread to the renderer. Loads and unloads share one
// queue so the renderer applies them in the order they were sent (#4).
struct ChunkMeshLoad
{
  MeshAllocation allocation;
};

struct ChunkMeshUnload
{
  glm::ivec3 pos;
};

using ChunkMeshMessage = std::variant<ChunkMeshLoad, ChunkMeshUnload>;

class MeshAllocator
{
private:
  VkContext &vkContext;
  vk::raii::Buffer &vertexBuffer;
  vk::raii::Buffer &indexBuffer;
  ArenaAllocator vertexAllocator;
  ArenaAllocator indexAllocator;
  vk::raii::Buffer stagingBuffer;
  vk::raii::DeviceMemory stagingMemory;
  uint8_t *stagingData;
  uint32_t stagingOffset = 0;
  vk::raii::CommandPool commandPool;
  vk::raii::CommandBuffer commandBuffer;
  vk::raii::Fence fence;
  bool recording = false;
  bool inFlight = false;

  vk::raii::CommandPool createCommandPool();
  vk::raii::CommandBuffer createCommandBuffer();
  void stage(const void *srcData, uint32_t srcSize, vk::raii::Buffer &dstBuffer, uint32_t dstOffset);

public:
  MeshAllocator(VkResource &resource, vk::raii::Buffer &vertexBuffer, vk::raii::Buffer &indexBuffer);
  ~MeshAllocator();
  MeshAllocator(const MeshAllocator &) = delete;
  MeshAllocator &operator=(const MeshAllocator &) = delete;

  MeshAllocation allocate(std::vector<ChunkVertex> &vertices, std::vector<uint32_t> &indices);
  void free(MeshAllocation &allocation);
  bool needsFlush(size_t vertexCount, size_t indexCount) const;
  void flush();
};

#endif // MESH_ALLOCATOR_HPP
