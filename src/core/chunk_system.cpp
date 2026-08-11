#include <core/chunk_system.hpp>
#include <array>
#include <core/math.hpp>
#include <core/chunk.hpp>
#include <core/voxel.hpp>
#include <core/chunk_mesh.hpp>
#include <iostream>

namespace
{
  constexpr int VIEW_DISTANCE = 12;
  constexpr int LOAD_DISTANCE = VIEW_DISTANCE + 1;

  constexpr std::array<glm::ivec3, 6> NEIGHBOR_OFFSETS = {{
      {1, 0, 0},
      {-1, 0, 0},
      {0, 1, 0},
      {0, -1, 0},
      {0, 0, 1},
      {0, 0, -1},
  }};

  bool neighborsResolved(World &world, const glm::ivec3 &pos)
  {
    for (const auto &offset : NEIGHBOR_OFFSETS)
    {
      const int ny = pos.y + offset.y;

      // if (ny < MIN_CHUNK_Y || ny > MAX_CHUNK_Y)
      //   continue;

      if (!world.isChunkResolved(pos.x + offset.x, ny, pos.z + offset.z))
        return false;
    }

    return true;
  }

  void dirtyChunkMesh(Chunk *chunk)
  {
    if (chunk)
      chunk->isMeshed = false;
  }
} // namespace

ChunkSystem::ChunkSystem(VkResource &resource, Camera &cam, World &w, ThreadQueue<MeshAllocation> &lQueue, ThreadQueue<glm::ivec3> &uQueue, std::pair<vk::raii::Buffer &, vk::raii::Buffer &> b)
    : vkResource(resource),
      camera(cam),
      loadQueue(lQueue),
      unloadQueue(uQueue),
      world(w),
      chunkMeshGenerator(w),
      buffers(b),
      meshAllocator(vkResource, buffers.first, buffers.second) {}

std::unordered_set<glm::ivec2> ChunkSystem::getChunksAround(glm::vec3 &position)
{
  constexpr int chunkWorldSize = CHUNK_SIZE * VOXEL_SIZE;

  std::unordered_set<glm::ivec2> coords;

  int centerX = floorDiv((int)position.x, chunkWorldSize);
  int centerZ = floorDiv((int)position.z, chunkWorldSize);

  for (int cx = centerX - LOAD_DISTANCE; cx <= centerX + LOAD_DISTANCE; cx++)
  {
    for (int cz = centerZ - LOAD_DISTANCE; cz <= centerZ + LOAD_DISTANCE; cz++)
    {
      int dx = cx - centerX;
      int dz = cz - centerZ;

      if (dx * dx + dz * dz <= LOAD_DISTANCE * LOAD_DISTANCE)
      {
        coords.emplace(glm::ivec2{cx, cz});
      }
    }
  }

  return coords;
}

void ChunkSystem::update()
{
  // static int avgVerticesPerChunk = 0;

  glm::vec3 &cameraPosition = camera.position;
  // glm::vec3 cameraPosition = glm::vec3(0, 0, 0);

  constexpr int chunkWorldSize = CHUNK_SIZE * VOXEL_SIZE;
  int centerX = floorDiv((int)cameraPosition.x, chunkWorldSize);
  int centerZ = floorDiv((int)cameraPosition.z, chunkWorldSize);

  auto desiredChunks = getChunksAround(cameraPosition);

  std::unordered_set<glm::ivec2> dirtyChunks;

  for (auto &coord : desiredChunks)
  {
    world.loadChunk(coord.x, coord.y);

    if (!loadedChunks.contains(coord))
    {
      // mark neighboruing chunks as dirty

      std::array<glm::ivec2, 4> offs = {{
          {1, 0},
          {-1, 0},
          {0, 1},
          {0, -1},
      }};

      for (auto &offset : offs)
      {
        dirtyChunks.emplace(coord + offset);
      }
    }
  }

  for (auto &coord : desiredChunks)
  {
    auto yChunks = world.getRenderChunks(coord.x, coord.y);

    for (auto *chunk : yChunks)
    {
      if (!chunk->isMeshed || dirtyChunks.contains(coord))
      {
        // if (!neighborsResolved(world, chunk->pos))
        //   continue;

        chunk->isMeshed = true;

        auto mesh = chunkMeshGenerator.getChunkMesh(*chunk, 1);
        auto it = meshAllocations.find(chunk->pos);
        auto meshAllocation = meshAllocator.allocate(mesh.vertices, mesh.indices);
        meshAllocation.pos = chunk->pos;

        if (it != meshAllocations.end())
        {
          meshAllocator.free(it->second);
          it->second = meshAllocation;
        }
        else
        {
          meshAllocations.emplace(chunk->pos, meshAllocation);
        }

        // assert(inserted);

        // avgVerticesPerChunk += mesh.vertices.size();
        // avgVerticesPerChunk /= 2;

        loadQueue.push(std::move(meshAllocation));
      }
    }

    dirtyChunks.erase(coord);

    loadedChunks.emplace(coord);
  }

  for (auto &coord : loadedChunks)
  {
    if (!desiredChunks.contains(coord))
    {
      auto yChunks = world.getRenderChunks(coord.x, coord.y);

      for (auto *chunk : yChunks)
      {
        chunk->isMeshed = false;
        glm::ivec3 pos = chunk->pos;

        auto meshAllocationNode = meshAllocations.extract(pos);

        assert(!meshAllocationNode.empty());

        meshAllocator.free(meshAllocationNode.mapped());
        unloadQueue.push(std::move(pos));
      }
    }
  }

  loadedChunks = desiredChunks;

  // std::cout << avgVerticesPerChunk << std::endl;
}
