#ifndef UBO_HPP
#define UBO_HPP

#include <cstddef>
#include <glm/glm.hpp>

struct UniformBufferObject
{
  glm::mat4 view;
  glm::mat4 projection;
  glm::vec3 cameraPosition;
  float deltaTime;
};

static_assert(offsetof(UniformBufferObject, view) == 0);
static_assert(offsetof(UniformBufferObject, projection) == 64);
static_assert(offsetof(UniformBufferObject, cameraPosition) == 128);
static_assert(offsetof(UniformBufferObject, deltaTime) == 140);
static_assert(sizeof(UniformBufferObject) == 144);

#endif // UBO_HPP
