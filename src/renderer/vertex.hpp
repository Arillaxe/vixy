#ifndef VERTEX_HPP
#define VERTEX_HPP

#include <glm/glm.hpp>

struct Vertex
{
  glm::vec3 position;
  glm::vec2 texCoord;
  glm::vec3 normal;
  uint32_t textureId;
};

#endif // VERTEX_HPP
