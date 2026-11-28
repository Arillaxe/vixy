#ifndef FRUSTUM_CULLER_HPP
#define FRUSTUM_CULLER_HPP

#include <core/camera.hpp>

struct Plane
{
  glm::vec3 normal;
  float d;
};

class FrustumCuller
{
public:
  FrustumCuller(const Camera &camera, float aspectRatio);

  bool isVisible(const glm::ivec3 &chunkPos) const;
  const Plane (&getPlanes() const)[6] { return planes; }

private:
  Plane planes[6];

  bool testAABB(const glm::vec3 &min, const glm::vec3 &max) const;
};

#endif // FRUSTUM_CULLER_HPP
