#include <core/world.hpp>
#include <core/chunk_generator.hpp>
#include <core/block.hpp>
#include <core/math.hpp>

World::World(uint32_t seed) : chunkGenerator(seed)
{
}

void World::loadColumn(int x, int z)
{
  glm::ivec2 columnPos{x, z};

  if (columns.contains(columnPos))
    return;

  Column column;

  auto savedIt = savedColumns.find(columnPos);

  if (savedIt != savedColumns.end())
  {
    for (auto &chunk : savedIt->second)
    {
      chunk.isMeshed = false;
      column.chunkYs.push_back(chunk.pos.y);
      chunks.insert_or_assign(chunk.pos, std::move(chunk));
    }

    savedColumns.erase(savedIt);
  }

  Heightmap heightmap = chunkGenerator.getHeightmap(x, z);
  std::vector<int> surfaceChunkYs = chunkGenerator.getRenderChunksYs(x, z, heightmap);

  column.minSurfaceChunkY = surfaceChunkYs.front();

  for (int y : surfaceChunkYs)
  {
    glm::ivec3 pos{x, y, z};

    if (chunks.contains(pos))
      continue;

    chunks.emplace(pos, chunkGenerator.generateChunk(x, y, z, heightmap));
    column.chunkYs.push_back(y);
  }

  columns.emplace(columnPos, std::move(column));
}

void World::unloadColumn(int x, int z)
{
  glm::ivec2 columnPos{x, z};

  auto columnIt = columns.find(columnPos);

  if (columnIt == columns.end())
    return;

  std::vector<Chunk> edited;

  for (int y : columnIt->second.chunkYs)
  {
    auto node = chunks.extract(glm::ivec3{x, y, z});

    if (!node.empty() && node.mapped().isEdited)
      edited.push_back(std::move(node.mapped()));
  }

  if (!edited.empty())
    savedColumns.insert_or_assign(columnPos, std::move(edited));

  columns.erase(columnIt);
}

bool World::isChunkResolved(int x, int y, int z) const
{
  return chunks.contains(glm::ivec3{x, y, z});
}

bool World::isImplicitSolid(int x, int y, int z) const
{
  auto columnIt = columns.find({x, z});

  if (columnIt == columns.end())
    return false;

  return y < columnIt->second.minSurfaceChunkY && !chunks.contains(glm::ivec3{x, y, z});
}

bool World::isSolid(int x, int y, int z) const
{
  const int chunkX = floorDiv(x, CHUNK_SIZE);
  const int chunkY = floorDiv(y, CHUNK_SIZE);
  const int chunkZ = floorDiv(z, CHUNK_SIZE);

  auto it = chunks.find(glm::ivec3{chunkX, chunkY, chunkZ});

  if (it == chunks.end())
    return isImplicitSolid(chunkX, chunkY, chunkZ);

  const int index = floorMod(x, CHUNK_SIZE) * CHUNK_SIZE * CHUNK_SIZE +
                    floorMod(y, CHUNK_SIZE) * CHUNK_SIZE +
                    floorMod(z, CHUNK_SIZE);

  return it->second.voxels[index].type != BLOCK_AIR;
}

Voxel *World::getVoxel(int x, int y, int z)
{
  const int chunkX = floorDiv(x, CHUNK_SIZE);
  const int chunkY = floorDiv(y, CHUNK_SIZE);
  const int chunkZ = floorDiv(z, CHUNK_SIZE);

  glm::ivec3 pos{chunkX, chunkY, chunkZ};

  auto it = chunks.find(pos);

  if (it == chunks.end())
    return nullptr;

  const int localX = floorMod(x, CHUNK_SIZE);
  const int localY = floorMod(y, CHUNK_SIZE);
  const int localZ = floorMod(z, CHUNK_SIZE);

  return &it->second.getVoxel(localX, localY, localZ);
}

std::vector<Chunk *> World::getColumnChunks(int x, int z)
{
  std::vector<Chunk *> ret;

  auto columnIt = columns.find({x, z});

  if (columnIt == columns.end())
    return ret;

  for (auto &y : columnIt->second.chunkYs)
  {
    auto chunkIt = chunks.find({x, y, z});

    if (chunkIt != chunks.end())
    {
      ret.push_back(&chunkIt->second);
    }
  }

  return ret;
}

Chunk *World::getChunk(int x, int y, int z)
{
  glm::ivec3 pos{x, y, z};

  auto chunkIt = chunks.find(pos);

  if (chunkIt == chunks.end())
  {
    return nullptr;
  }

  return &chunkIt->second;
}

Chunk *World::ensureChunk(int x, int y, int z)
{
  glm::ivec3 pos{x, y, z};

  if (Chunk *existing = getChunk(x, y, z))
    return existing;

  auto columnIt = columns.find({x, z});

  if (columnIt == columns.end())
    return nullptr;

  Chunk chunk;

  if (y < columnIt->second.minSurfaceChunkY)
  {
    chunk = chunkGenerator.generateChunk(x, y, z, chunkGenerator.getHeightmap(x, z));
  }
  else
  {
    chunk.pos = pos;
    chunk.voxels.fill(Voxel{BLOCK_AIR});
  }

  columnIt->second.chunkYs.push_back(y);

  return &chunks.emplace(pos, std::move(chunk)).first->second;
}
