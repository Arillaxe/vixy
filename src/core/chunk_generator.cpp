#include <core/chunk_generator.hpp>
#include <core/voxel.hpp>
#include <core/block.hpp>
#include <algorithm>
#include <core/math.hpp>

constexpr int MAX_WORLD_HEIGHT = 256;
constexpr double continentalFrequency = 0.002;
constexpr double hillFrequency = 0.008;
constexpr double detailFrequency = 0.03;

constexpr int baseHeight = 48;
constexpr int continentalAmplitude = 40;
constexpr int hillAmplitude = 14;
constexpr int detailAmplitude = 3;

ChunkGenerator::ChunkGenerator(uint32_t seed) : perlin(seed)
{
}

int ChunkGenerator::getWorldHeight(int x, int y)
{
  double continentalNoise = perlin.octave2D_01(
      x * continentalFrequency,
      y * continentalFrequency,
      4);

  // Medium-sized hills.
  double hillNoise = perlin.octave2D_01(
      x * hillFrequency,
      y * hillFrequency,
      4);

  // Small surface variation.
  double detailNoise = perlin.octave2D_01(
      x * detailFrequency,
      y * detailFrequency,
      2);

  // Convert noise from [0, 1] to approximately [-1, 1].
  double continental = continentalNoise * 2.0 - 1.0;
  double hills = hillNoise * 2.0 - 1.0;
  double detail = detailNoise * 2.0 - 1.0;

  return baseHeight +
         static_cast<int>(continental * continentalAmplitude) +
         static_cast<int>(hills * hillAmplitude) +
         static_cast<int>(detail * detailAmplitude);
}

Heightmap ChunkGenerator::getHeightmap(int x, int z)
{
  Heightmap heightmap;

  for (int localX = 0; localX < CHUNK_SIZE; ++localX)
  {
    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ)
    {
      int worldX = x * CHUNK_SIZE + localX;
      int worldZ = z * CHUNK_SIZE + localZ;

      heightmap[localX * CHUNK_SIZE + localZ] = getWorldHeight(worldX, worldZ);
    }
  }

  return heightmap;
}

std::vector<int> ChunkGenerator::getRenderChunksYs(const Heightmap &heightmap)
{
  std::vector<int> chunkYs;

  for (int height : heightmap)
    chunkYs.push_back(floorDiv(height, CHUNK_SIZE));

  std::sort(chunkYs.begin(), chunkYs.end());
  chunkYs.erase(std::unique(chunkYs.begin(), chunkYs.end()), chunkYs.end());

  return chunkYs;
}

Chunk ChunkGenerator::generateChunk(int chunkX, int chunkY, int chunkZ, const Heightmap &heightmap)
{
  Chunk chunk;

  chunk.isMeshed = false;
  chunk.voxelCount = 0;
  chunk.pos = {chunkX, chunkY, chunkZ};

  int chunkBottom = chunkY * CHUNK_SIZE;
  int chunkTop = chunkBottom + CHUNK_SIZE - 1;

  for (int localX = 0; localX < CHUNK_SIZE; ++localX)
  {
    for (int localZ = 0; localZ < CHUNK_SIZE; ++localZ)
    {
      int worldHeight = heightmap[localX * CHUNK_SIZE + localZ];

      for (int localY = 0; localY < CHUNK_SIZE; ++localY)
      {
        int worldY = chunkBottom + localY;

        Voxel &voxel = chunk.getVoxel(localX, localY, localZ);

        if (worldY < worldHeight)
        {
          voxel.type = BLOCK_DIRT;
          ++chunk.voxelCount;
        }
        else if (worldY == worldHeight)
        {
          voxel.type = BLOCK_GRASS;
          ++chunk.voxelCount;
        }
        else
        {
          voxel.type = BLOCK_AIR;
        }
      }
    }
  }

  return chunk;
}
