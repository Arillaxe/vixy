#ifndef CHUNK_GENERATOR_HPP
#define CHUNK_GENERATOR_HPP

#include <perlin_noise.hpp>
#include <core/chunk.hpp>

typedef std::array<int, CHUNK_SIZE * CHUNK_SIZE> Heightmap;

class ChunkGenerator
{
private:
  siv::PerlinNoise perlin;

  int getWorldHeight(int x, int y);

public:
  ChunkGenerator(uint32_t seed);

  Heightmap getHeightmap(int x, int z);
  std::vector<int> getRenderChunksYs(int x, int z, const Heightmap &heightmap);
  Chunk generateChunk(int x, int y, int z, const Heightmap &heightmap);
};

#endif // CHUNK_GENERATOR_HPP
