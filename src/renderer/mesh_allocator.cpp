#include <renderer/mesh_allocator.hpp>
#include <stdexcept>

namespace
{
  constexpr uint32_t STAGING_ALIGNMENT = 16;

  uint32_t alignUp(uint32_t value)
  {
    return (value + STAGING_ALIGNMENT - 1) & ~(STAGING_ALIGNMENT - 1);
  }
} // namespace

MeshAllocator::MeshAllocator(VkResource &resource, vk::raii::Buffer &vBuffer, vk::raii::Buffer &iBuffer)
    : vkContext(resource.getContext()),
      vertexBuffer(vBuffer),
      indexBuffer(iBuffer),
      vertexAllocator(VERTEX_BUFFER_SIZE),
      indexAllocator(INDEX_BUFFER_SIZE),
      stagingBuffer(resource.createBuffer(STAGING_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferSrc)),
      stagingMemory(resource.getBufferMemory(stagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)),
      stagingData(static_cast<uint8_t *>(stagingMemory.mapMemory(0, STAGING_BUFFER_SIZE))),
      commandPool(createCommandPool()),
      commandBuffer(createCommandBuffer()),
      fence(vkContext.getDevice(), vk::FenceCreateInfo{}) {}

MeshAllocator::~MeshAllocator()
{
  if (!inFlight)
    return;

  try
  {
    (void)vkContext.getDevice().waitForFences(*fence, vk::True, UINT64_MAX);
  }
  catch (...)
  {
  }
}

vk::raii::CommandPool MeshAllocator::createCommandPool()
{
  vk::CommandPoolCreateInfo poolInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = vkContext.getTransferQueueFamilyIndex(),
  };

  return vk::raii::CommandPool(vkContext.getDevice(), poolInfo);
}

vk::raii::CommandBuffer MeshAllocator::createCommandBuffer()
{
  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = *commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };

  return std::move(vk::raii::CommandBuffers(vkContext.getDevice(), allocInfo).front());
}

void MeshAllocator::stage(const void *srcData, uint32_t srcSize, vk::raii::Buffer &dstBuffer, uint32_t dstOffset)
{
  if (!recording)
  {
    vk::CommandBufferBeginInfo beginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    };
    commandBuffer.begin(beginInfo);
    recording = true;
  }

  memcpy(stagingData + stagingOffset, srcData, srcSize);

  vk::BufferCopy copyParams{
      .srcOffset = stagingOffset,
      .dstOffset = dstOffset,
      .size = srcSize,
  };
  commandBuffer.copyBuffer(*stagingBuffer, *dstBuffer, copyParams);

  stagingOffset += alignUp(srcSize);
}

void MeshAllocator::flush()
{
  if (!recording)
    return;

  commandBuffer.end();
  recording = false;

  vk::SubmitInfo submitInfo{
      .commandBufferCount = 1,
      .pCommandBuffers = &*commandBuffer,
  };

  {
    std::lock_guard lock(vkContext.getTransferQueueMutex());
    vkContext.getTransferQueue().submit(submitInfo, *fence);
  }
  inFlight = true;

  if (vkContext.getDevice().waitForFences(*fence, vk::True, UINT64_MAX) != vk::Result::eSuccess)
    throw std::runtime_error("Chunk upload fence wait failed");

  inFlight = false;
  vkContext.getDevice().resetFences(*fence);
  stagingOffset = 0;
}

bool MeshAllocator::needsFlush(size_t vertexCount, size_t indexCount) const
{
  uint32_t stagingSize = alignUp(sizeof(ChunkVertex) * vertexCount) + alignUp(sizeof(uint32_t) * indexCount);

  return stagingOffset + stagingSize > STAGING_BUFFER_SIZE;
}

MeshAllocation MeshAllocator::allocate(std::vector<ChunkVertex> &vertices, std::vector<uint32_t> &indices)
{
  uint32_t verticesAllocationSize = sizeof(ChunkVertex) * vertices.size();
  uint32_t indicesAllocationSize = sizeof(uint32_t) * indices.size();
  uint32_t stagingSize = alignUp(verticesAllocationSize) + alignUp(indicesAllocationSize);

  if (stagingSize > STAGING_BUFFER_SIZE)
  {
    throw std::runtime_error("Mesh too large for staging buffer");
  }

  if (needsFlush(vertices.size(), indices.size()))
  {
    flush();
  }

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

  stage(vertices.data(), verticesAllocationSize, vertexBuffer, verticesOffset);
  stage(indices.data(), indicesAllocationSize, indexBuffer, indicesOffset);

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
  vertexAllocator.free(allocation.vertexOffset, sizeof(ChunkVertex) * allocation.vertexCount);
  indexAllocator.free(allocation.indexOffset, sizeof(uint32_t) * allocation.indexCount);
}
