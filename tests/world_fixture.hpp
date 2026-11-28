#ifndef WORLD_FIXTURE_HPP
#define WORLD_FIXTURE_HPP

#include <core/world.hpp>
#include <core/block.hpp>
#include <core/math.hpp>

inline Chunk &addEmptyChunk(World &world, glm::ivec3 pos)
{
  Chunk &chunk = world.getChunks()[pos];
  chunk.pos = pos;
  chunk.voxels.fill(Voxel{BLOCK_AIR});
  return chunk;
}

inline void setBlock(World &world, glm::ivec3 voxel, BlockType type)
{
  glm::ivec3 chunkPos{
      floorDiv(voxel.x, CHUNK_SIZE),
      floorDiv(voxel.y, CHUNK_SIZE),
      floorDiv(voxel.z, CHUNK_SIZE)};

  Chunk *chunk = world.getChunk(chunkPos.x, chunkPos.y, chunkPos.z);

  if (!chunk)
    chunk = &addEmptyChunk(world, chunkPos);

  chunk->getVoxel(
           floorMod(voxel.x, CHUNK_SIZE),
           floorMod(voxel.y, CHUNK_SIZE),
           floorMod(voxel.z, CHUNK_SIZE))
      .type = static_cast<uint8_t>(type);
}

#endif // WORLD_FIXTURE_HPP
