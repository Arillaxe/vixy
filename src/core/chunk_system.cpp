#include <core/chunk_system.hpp>
#include <array>
#include <core/math.hpp>
#include <core/chunk.hpp>
#include <core/voxel.hpp>
#include <core/chunk_mesh.hpp>
#include <core/raycast.hpp>
#include <iostream>

namespace
{
  constexpr int VIEW_DISTANCE = 24;
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

ChunkSystem::ChunkSystem(VkResource &resource, glm::vec3 initialViewerPosition, World &w, ThreadQueue<ChunkMeshMessage> &mQueue, ThreadQueue<MeshAllocation> &fQueue, std::pair<vk::raii::Buffer &, vk::raii::Buffer &> b)
    : vkResource(resource),
      world(w),
      chunkMeshGenerator(w),
      meshQueue(mQueue),
      freeQueue(fQueue),
      viewerPosition(initialViewerPosition),
      buffers(b),
      meshAllocator(vkResource, buffers.first, buffers.second) {}

void ChunkSystem::run()
{
  try
  {
    loop();
  }
  catch (...)
  {
    error = std::current_exception();
    failed.store(true, std::memory_order_release);
  }
}

void ChunkSystem::rethrowIfFailed()
{
  if (failed.load(std::memory_order_acquire))
    std::rethrow_exception(error);
}

void ChunkSystem::loop()
{
  constexpr auto tickInterval = std::chrono::milliseconds(16);

  auto nextTick = std::chrono::steady_clock::now();

  while (!shouldClose)
  {
    processEdits();

    if (std::chrono::steady_clock::now() >= nextTick)
    {
      auto tickStart = std::chrono::steady_clock::now();

      update();

      nextTick = tickStart + tickInterval;
    }

    // Sleep until the next tick, but wake early for a new edit or close().
    std::unique_lock lock(wakeMutex);
    wakeCondition.wait_until(lock, nextTick, [this]
                             { return wakeRequested || shouldClose; });
    wakeRequested = false;
  }
}

void ChunkSystem::close()
{
  shouldClose = true;
  wake();
}

void ChunkSystem::wake()
{
  {
    std::lock_guard lock(wakeMutex);
    wakeRequested = true;
  }

  wakeCondition.notify_one();
}

void ChunkSystem::pushEdit(BlockEdit edit)
{
  editQueue.push(std::move(edit));
  wake();
}

void ChunkSystem::processFrees()
{
  MeshAllocation allocation{};

  while (freeQueue.tryPop(allocation))
  {
    meshAllocator.free(allocation);
  }
}

void ChunkSystem::processEdits()
{
  processFrees();

  BlockEdit edit{};

  while (editQueue.tryPop(edit))
  {
    applyEdit(edit);
  }
}

void ChunkSystem::setViewerPosition(const glm::vec3 &position)
{
  std::lock_guard lock(viewerMutex);
  viewerPosition = position;
}

glm::vec3 ChunkSystem::getViewerPosition()
{
  std::lock_guard lock(viewerMutex);
  return viewerPosition;
}

void ChunkSystem::applyEdit(const BlockEdit &edit)
{
  constexpr float MAX_REACH = 8.0f;

  auto hit = raycast(world, edit.origin, edit.direction, MAX_REACH);

  if (!hit.hit)
    return;

  const bool isBreak = edit.action == BlockEdit::Action::Break;
  const glm::ivec3 voxelPos = isBreak ? hit.voxel : hit.previousVoxel;

  const glm::ivec3 chunkPos{
      floorDiv(voxelPos.x, CHUNK_SIZE),
      floorDiv(voxelPos.y, CHUNK_SIZE),
      floorDiv(voxelPos.z, CHUNK_SIZE),
  };

  if (!isBreak)
    world.createEmptyChunk(chunkPos.x, chunkPos.y, chunkPos.z);

  Voxel *voxel = world.getVoxel(voxelPos.x, voxelPos.y, voxelPos.z);

  if (!voxel)
    return;

  voxel->type = isBreak ? 0 : 1;

  for (const auto &offset : NEIGHBOR_OFFSETS)
  {
    const glm::ivec3 neighborPos = chunkPos + offset;

    if (world.getChunk(neighborPos.x, neighborPos.y, neighborPos.z))
      updateChunk(neighborPos);
  }

  updateChunk(chunkPos);
}

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

  processFrees();

  glm::vec3 cameraPosition = getViewerPosition();

  constexpr int chunkWorldSize = CHUNK_SIZE * VOXEL_SIZE;
  int centerX = floorDiv((int)cameraPosition.x, chunkWorldSize);
  int centerZ = floorDiv((int)cameraPosition.z, chunkWorldSize);

  auto desiredChunks = getChunksAround(cameraPosition);

  std::unordered_set<glm::ivec2> dirtyChunks;

  for (auto &coord : desiredChunks)
  {
    // A full pass over every column is slow, so apply pending block edits
    // between columns instead of making them wait for the whole pass.
    processEdits();

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
    processEdits();

    auto yChunks = world.getRenderChunks(coord.x, coord.y);

    for (auto *chunk : yChunks)
    {
      if (!chunk->isMeshed || dirtyChunks.contains(coord))
      {
        // if (!neighborsResolved(world, chunk->pos))
        //   continue;

        remeshChunk(*chunk);

        // avgVerticesPerChunk += mesh.vertices.size();
        // avgVerticesPerChunk /= 2;
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

        if (meshAllocationNode.empty())
          continue;

        meshQueue.push(ChunkMeshUnload{pos});
      }
    }
  }

  loadedChunks = desiredChunks;

  // std::cout << avgVerticesPerChunk << std::endl;
}

void ChunkSystem::updateChunk(glm::ivec3 chunkPos)
{
  auto *chunk = world.getChunk(chunkPos.x, chunkPos.y, chunkPos.z);

  remeshChunk(*chunk);
}

void ChunkSystem::remeshChunk(Chunk &chunk)
{
  chunk.isMeshed = true;

  auto mesh = chunkMeshGenerator.getChunkMesh(chunk, 1);
  auto it = meshAllocations.find(chunk.pos);

  if (mesh.vertices.empty() || mesh.indices.empty())
  {
    if (it != meshAllocations.end())
    {
      meshAllocations.erase(it);
      meshQueue.push(ChunkMeshUnload{chunk.pos});
    }

    return;
  }

  auto meshAllocation = meshAllocator.allocate(mesh.vertices, mesh.indices);
  meshAllocation.pos = chunk.pos;

  if (it != meshAllocations.end())
  {
    it->second = meshAllocation;
  }
  else
  {
    meshAllocations.emplace(chunk.pos, meshAllocation);
  }

  meshQueue.push(ChunkMeshLoad{meshAllocation});
}
