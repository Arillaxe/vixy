#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Camera
{
  glm::vec3 position;
  glm::quat rotation;
  float fieldOfView = 45.0f;
  float nearPlane = 1.0f;
  float farPlane = 100000.0f;

  glm::mat4 viewMatrix() const
  {
    return glm::mat4_cast(glm::conjugate(rotation)) *
           glm::translate(glm::mat4(1.0f), -position);
  }

  glm::mat4 projectionMatrix(float aspectRatio) const
  {
    return glm::perspective(glm::radians(fieldOfView), aspectRatio, farPlane, nearPlane);
  }
};

#endif // CAMERA_HPP
