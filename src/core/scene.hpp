#ifndef SCENE_HPP
#define SCENE_HPP

#include <ecs/entity.hpp>

class Scene
{
private:
  std::vector<std::unique_ptr<Entity>> entities;

public:
  void addEntity(std::unique_ptr<Entity> entity) { entities.push_back(std::move(entity)); }
  std::vector<std::unique_ptr<Entity>> &getEntities() { return entities; }
};

#endif // SCENE_HPP
