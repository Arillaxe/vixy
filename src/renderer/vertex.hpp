#ifndef VERTEX_HPP
#define VERTEX_HPP

#include <glm/glm.hpp>
#include <cstddef>

struct Vertex
{
  glm::vec3 position;
  glm::vec2 texCoord;
  glm::vec3 normal;
  uint32_t textureId;
};

struct ChunkVertex
{
  uint32_t data;
  uint32_t textureId;

  static constexpr uint32_t pack(uint32_t x, uint32_t y, uint32_t z, uint32_t u, uint32_t v, uint32_t face)
  {
    return x | (y << 5) | (z << 10) | (u << 15) | (v << 20) | (face << 25);
  }
};

static_assert(sizeof(ChunkVertex) == 8);

struct ChunkInstance
{
  glm::vec3 origin;
  float scale;
};

static_assert(sizeof(ChunkInstance) == 16 && offsetof(ChunkInstance, scale) == 12);

#endif // VERTEX_HPP
