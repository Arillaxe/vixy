#ifndef WORLD_HPP
#define WORLD_HPP

#include <unordered_map>
#include <unordered_set>
#include <core/voxel.hpp>
#include <core/chunk.hpp>
#include <core/chunk_generator.hpp>

class World
{
private:
  ChunkGenerator chunkGenerator;
  std::unordered_map<glm::ivec3, Chunk> chunks;
  std::unordered_map<glm::ivec2, std::vector<int>> columnChunkYs;
  std::unordered_map<glm::ivec2, std::vector<Chunk>> savedColumns;

public:
  World(uint32_t seed);
  std::unordered_map<glm::ivec3, Chunk> &getChunks() { return chunks; }

  void loadColumn(int x, int z);
  void unloadColumn(int x, int z);
  bool isChunkResolved(int x, int y, int z) const;
  std::vector<Chunk *> getColumnChunks(int x, int z);
  Chunk *getChunk(int x, int y, int z);
  Voxel *getVoxel(int x, int y, int z);
  Chunk *createEmptyChunk(int x, int y, int z);
};

#endif // WORLD_HPP
