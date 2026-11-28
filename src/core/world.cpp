#include <core/world.hpp>
#include <core/chunk_generator.hpp>
#include <core/math.hpp>

World::World(uint32_t seed) : chunkGenerator(seed)
{
}

void World::loadColumn(int x, int z)
{
  glm::ivec2 column{x, z};

  if (columnChunkYs.contains(column))
    return;

  std::vector<int> chunkYs;

  auto savedIt = savedColumns.find(column);

  if (savedIt != savedColumns.end())
  {
    for (auto &chunk : savedIt->second)
    {
      chunk.isMeshed = false;
      chunkYs.push_back(chunk.pos.y);
      chunks.insert_or_assign(chunk.pos, std::move(chunk));
    }

    savedColumns.erase(savedIt);
  }

  Heightmap heightmap = chunkGenerator.getHeightmap(x, z);

  for (int y : ChunkGenerator::getRenderChunksYs(heightmap))
  {
    glm::ivec3 pos{x, y, z};

    if (chunks.contains(pos))
      continue;

    chunks.emplace(pos, chunkGenerator.generateChunk(x, y, z, heightmap));
    chunkYs.push_back(y);
  }

  columnChunkYs.emplace(column, std::move(chunkYs));
}

void World::unloadColumn(int x, int z)
{
  glm::ivec2 column{x, z};

  auto columnIt = columnChunkYs.find(column);

  if (columnIt == columnChunkYs.end())
    return;

  std::vector<Chunk> edited;

  for (int y : columnIt->second)
  {
    auto node = chunks.extract(glm::ivec3{x, y, z});

    if (!node.empty() && node.mapped().isEdited)
      edited.push_back(std::move(node.mapped()));
  }

  if (!edited.empty())
    savedColumns.insert_or_assign(column, std::move(edited));

  columnChunkYs.erase(columnIt);
}

bool World::isChunkResolved(int x, int y, int z) const
{
  return chunks.contains(glm::ivec3{x, y, z});
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

  auto columnIt = columnChunkYs.find({x, z});

  if (columnIt == columnChunkYs.end())
    return ret;

  for (auto &y : columnIt->second)
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

Chunk *World::createEmptyChunk(int x, int y, int z)
{
  glm::ivec3 pos{x, y, z};

  if (Chunk *existing = getChunk(x, y, z))
    return existing;

  auto columnIt = columnChunkYs.find({x, z});

  if (columnIt == columnChunkYs.end())
    return nullptr;

  Chunk emptyChunk;
  emptyChunk.pos = pos;

  for (int i = 0; i < CHUNK_SIZE; i++)
  {
    for (int j = 0; j < CHUNK_SIZE; j++)
    {
      for (int k = 0; k < CHUNK_SIZE; k++)
      {
        emptyChunk.getVoxel(i, j, k) = {0};
      }
    }
  }

  columnIt->second.push_back(y);

  return &chunks.emplace(pos, std::move(emptyChunk)).first->second;
}
