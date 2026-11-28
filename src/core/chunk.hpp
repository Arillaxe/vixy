#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <core/voxel.hpp>
#include <cassert>
#include <glm/gtx/hash.hpp>

constexpr int CHUNK_SIZE = 16;

typedef std::array<Voxel, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> Voxels;

class Chunk
{
public:
  glm::ivec3 pos;
  Voxels voxels;
  bool isMeshed = false;
  bool isEdited = false;
  uint32_t voxelCount = 0;

  Voxel &getVoxel(int x, int y, int z)
  {
    assert(x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE);
    return voxels[x * CHUNK_SIZE * CHUNK_SIZE + y * CHUNK_SIZE + z];
  }
};

#endif // CHUNK_HPP
