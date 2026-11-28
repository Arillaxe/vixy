#include <core/chunk_system.hpp>
#include <array>
#include <core/math.hpp>
#include <core/chunk.hpp>
#include <core/voxel.hpp>
#include <core/block.hpp>
#include <core/chunk_mesh.hpp>
#include <core/raycast.hpp>
#include <iostream>

namespace
{
  constexpr int VIEW_DISTANCE = 24;
  constexpr int LOAD_DISTANCE = VIEW_DISTANCE + 1;
  constexpr auto UPLOAD_FLUSH_INTERVAL = std::chrono::milliseconds(16);

  constexpr std::array<glm::ivec3, 6> NEIGHBOR_OFFSETS = {{
      {1, 0, 0},
      {-1, 0, 0},
      {0, 1, 0},
      {0, -1, 0},
      {0, 0, 1},
      {0, 0, -1},
  }};
} // namespace

ChunkSystem::ChunkSystem(GpuResource &resource, glm::vec3 initialViewerPosition, World &w, ThreadQueue<ChunkMeshMessage> &mQueue, ThreadQueue<MeshAllocation> &fQueue, std::pair<vk::raii::Buffer &, vk::raii::Buffer &> b)
    : gpuResource(resource),
      world(w),
      chunkMeshGenerator(w),
      meshQueue(mQueue),
      freeQueue(fQueue),
      viewerPosition(initialViewerPosition),
      buffers(b),
      meshAllocator(gpuResource, buffers.first, buffers.second) {}

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
  bool applied = false;

  while (editQueue.tryPop(edit))
  {
    applyEdit(edit);
    applied = true;
  }

  if (applied)
    flushUploads();
}

void ChunkSystem::queueMessage(ChunkMeshMessage message)
{
  pendingMessages.push_back(std::move(message));
}

void ChunkSystem::flushUploads()
{
  meshAllocator.flush();

  for (auto &message : pendingMessages)
    meshQueue.push(std::move(message));

  pendingMessages.clear();
  lastFlush = std::chrono::steady_clock::now();
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

  if (edit.action == BlockEdit::Action::Break)
  {
    setVoxel(hit.voxel, BLOCK_AIR);
    return;
  }

  const glm::ivec3 originVoxel{
      static_cast<int>(std::floor(edit.origin.x / VOXEL_SIZE)),
      static_cast<int>(std::floor(edit.origin.y / VOXEL_SIZE)),
      static_cast<int>(std::floor(edit.origin.z / VOXEL_SIZE)),
  };

  if (hit.previousVoxel == originVoxel)
    return;

  setVoxel(hit.previousVoxel, BLOCK_GRASS);
}

void ChunkSystem::setVoxel(const glm::ivec3 &voxelPos, BlockType type)
{
  const glm::ivec3 chunkPos{
      floorDiv(voxelPos.x, CHUNK_SIZE),
      floorDiv(voxelPos.y, CHUNK_SIZE),
      floorDiv(voxelPos.z, CHUNK_SIZE),
  };

  Chunk *chunk = world.ensureChunk(chunkPos.x, chunkPos.y, chunkPos.z);

  if (!chunk)
    return;

  const glm::ivec3 localPos{
      floorMod(voxelPos.x, CHUNK_SIZE),
      floorMod(voxelPos.y, CHUNK_SIZE),
      floorMod(voxelPos.z, CHUNK_SIZE),
  };

  chunk->getVoxel(localPos.x, localPos.y, localPos.z).type = type;
  chunk->isEdited = true;

  for (const auto &offset : NEIGHBOR_OFFSETS)
  {
    const glm::ivec3 neighborLocal = localPos + offset;

    if (glm::all(glm::greaterThanEqual(neighborLocal, glm::ivec3(0))) &&
        glm::all(glm::lessThan(neighborLocal, glm::ivec3(CHUNK_SIZE))))
      continue;

    const glm::ivec3 neighborPos = chunkPos + offset;

    if (type == BLOCK_AIR && world.isImplicitSolid(neighborPos.x, neighborPos.y, neighborPos.z))
      world.ensureChunk(neighborPos.x, neighborPos.y, neighborPos.z)->isEdited = true;

    if (Chunk *neighbor = world.getChunk(neighborPos.x, neighborPos.y, neighborPos.z))
      remeshChunk(*neighbor);
  }

  remeshChunk(*chunk);
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
  processFrees();

  glm::vec3 cameraPosition = getViewerPosition();

  constexpr int chunkWorldSize = CHUNK_SIZE * VOXEL_SIZE;
  int centerX = floorDiv((int)cameraPosition.x, chunkWorldSize);
  int centerZ = floorDiv((int)cameraPosition.z, chunkWorldSize);

  auto desiredChunks = getChunksAround(cameraPosition);

  std::unordered_set<glm::ivec2> dirtyChunks;

  for (auto &coord : desiredChunks)
  {
    if (loadedChunks.contains(coord))
      continue;

    // A full pass over every column is slow, so apply pending block edits
    // between columns instead of making them wait for the whole pass.
    processEdits();

    world.loadColumn(coord.x, coord.y);

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

  for (auto &coord : desiredChunks)
  {
    if (loadedChunks.contains(coord) && !dirtyChunks.contains(coord))
      continue;

    processEdits();

    auto yChunks = world.getColumnChunks(coord.x, coord.y);

    for (auto *chunk : yChunks)
    {
      if (!chunk->isMeshed || dirtyChunks.contains(coord))
      {
        remeshChunk(*chunk);
      }
    }

    dirtyChunks.erase(coord);

    loadedChunks.emplace(coord);

    if (std::chrono::steady_clock::now() - lastFlush >= UPLOAD_FLUSH_INTERVAL)
      flushUploads();
  }

  for (auto &coord : loadedChunks)
  {
    if (desiredChunks.contains(coord))
      continue;

    for (auto *chunk : world.getColumnChunks(coord.x, coord.y))
    {
      if (meshAllocations.erase(chunk->pos))
        queueMessage(ChunkMeshUnload{chunk->pos});
    }

    world.unloadColumn(coord.x, coord.y);
  }

  loadedChunks = desiredChunks;

  flushUploads();
}

void ChunkSystem::remeshChunk(Chunk &chunk)
{
  chunk.isMeshed = true;

  auto mesh = chunkMeshGenerator.getChunkMesh(chunk);
  auto it = meshAllocations.find(chunk.pos);

  if (mesh.vertices.empty() || mesh.indices.empty())
  {
    if (it != meshAllocations.end())
    {
      meshAllocations.erase(it);
      queueMessage(ChunkMeshUnload{chunk.pos});
    }

    return;
  }

  if (meshAllocator.needsFlush(mesh.vertices.size(), mesh.indices.size()))
    flushUploads();

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

  queueMessage(ChunkMeshLoad{meshAllocation});
}
