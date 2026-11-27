#ifndef MESH_ALLOCATOR_HPP
#define MESH_ALLOCATOR_HPP

#include <cstdint>
#include <variant>
#include <core/arena_allocator.hpp>
#include <renderer/vertex.hpp>
#include <renderer/vk/vk_resource.hpp>

constexpr uint32_t VERTEX_BUFFER_SIZE = 64 * 1024 * 1024; // 64 MiB
constexpr uint32_t INDEX_BUFFER_SIZE = 32 * 1024 * 1024;  // 32 MiB

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
  VkResource &vkResource;
  vk::raii::Buffer &vertexBuffer;
  vk::raii::Buffer &indexBuffer;
  ArenaAllocator vertexAllocator;
  ArenaAllocator indexAllocator;

  void copyData(void *srcData, uint32_t srcSize, vk::raii::Buffer &dstBuffer, uint32_t dstOffset);

public:
  MeshAllocator(VkResource &resource, vk::raii::Buffer &vertexBuffer, vk::raii::Buffer &indexBuffer);
  MeshAllocation allocate(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);
  void free(MeshAllocation &allocation);
};

#endif // MESH_ALLOCATOR_HPP
