#include <renderer/mesh_allocator.hpp>
#include <stdexcept>

MeshAllocator::MeshAllocator(VkResource &resource, vk::raii::Buffer &vBuffer, vk::raii::Buffer &iBuffer)
    : vkResource(resource),
      vertexBuffer(vBuffer),
      indexBuffer(iBuffer),
      vertexAllocator(VERTEX_BUFFER_SIZE),
      indexAllocator(INDEX_BUFFER_SIZE) {}

void MeshAllocator::copyData(void *srcData, uint32_t srcSize, vk::raii::Buffer &dstBuffer, uint32_t dstOffset)
{
  auto stagingBuffer = vkResource.createBuffer(srcSize, vk::BufferUsageFlagBits::eTransferSrc);
  auto stagingBufferMemory = vkResource.getBufferMemory(stagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  void *dataStaging = stagingBufferMemory.mapMemory(0, srcSize);

  memcpy(dataStaging, srcData, srcSize);

  stagingBufferMemory.unmapMemory();

  vk::BufferCopy copyParams{
      .dstOffset = dstOffset,
      .size = srcSize,
  };

  vkResource.copyBufferEx(stagingBuffer, dstBuffer, copyParams);
}

MeshAllocation MeshAllocator::allocate(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices)
{
  uint32_t verticesAllocationSize = sizeof(Vertex) * vertices.size();
  uint32_t indicesAllocationSize = sizeof(uint32_t) * indices.size();

  uint32_t verticesOffset;

  if (!vertexAllocator.allocate(verticesAllocationSize, verticesOffset))
  {
    throw std::runtime_error("No vertex memory");
  }

  uint32_t indicesOffset;

  if (!indexAllocator.allocate(indicesAllocationSize, indicesOffset))
  {
    throw std::runtime_error("No index memory");
  }

  copyData(vertices.data(), sizeof(Vertex) * vertices.size(), vertexBuffer, verticesOffset);
  copyData(indices.data(), sizeof(uint32_t) * indices.size(), indexBuffer, indicesOffset);

  return MeshAllocation{
      {0, 0, 0},
      verticesOffset,
      static_cast<uint32_t>(vertices.size()),
      indicesOffset,
      static_cast<uint32_t>(indices.size()),
  };
}

void MeshAllocator::free(MeshAllocation &allocation)
{
  vertexAllocator.free(allocation.vertexOffset, sizeof(Vertex) * allocation.vertexCount);
  indexAllocator.free(allocation.indexOffset, sizeof(uint32_t) * allocation.indexCount);
}
