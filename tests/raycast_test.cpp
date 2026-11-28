#include <doctest.h>
#include <core/raycast.hpp>
#include <world_fixture.hpp>

namespace
{
glm::vec3 voxelToWorld(float x, float y, float z)
{
  return glm::vec3(x, y, z) * static_cast<float>(VOXEL_SIZE);
}
}

TEST_CASE("raycast misses in an empty world")
{
  World world(0);
  addEmptyChunk(world, {0, 0, 0});

  RaycastHit hit = raycast(world, voxelToWorld(0.5f, 0.5f, 0.5f), {1.0f, 0.0f, 0.0f}, 8.0f);

  CHECK_FALSE(hit.hit);
}

TEST_CASE("raycast treats missing chunks as air")
{
  World world(0);

  RaycastHit hit = raycast(world, voxelToWorld(0.5f, 0.5f, 0.5f), {1.0f, 1.0f, 1.0f}, 8.0f);

  CHECK_FALSE(hit.hit);
}

TEST_CASE("raycast hits along the positive x axis")
{
  World world(0);
  setBlock(world, {5, 3, 3}, BLOCK_DIRT);

  RaycastHit hit = raycast(world, voxelToWorld(0.5f, 3.5f, 3.5f), {1.0f, 0.0f, 0.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(5, 3, 3));
  CHECK(hit.previousVoxel == glm::ivec3(4, 3, 3));
  CHECK(hit.distance == doctest::Approx(4.5f * VOXEL_SIZE));
}

TEST_CASE("raycast does not hit beyond the maximum distance in voxels")
{
  World world(0);
  setBlock(world, {5, 3, 3}, BLOCK_DIRT);

  RaycastHit hit = raycast(world, voxelToWorld(0.5f, 3.5f, 3.5f), {1.0f, 0.0f, 0.0f}, 4.0f);

  CHECK_FALSE(hit.hit);
}

TEST_CASE("raycast normalizes the direction")
{
  World world(0);
  setBlock(world, {3, 8, 3}, BLOCK_DIRT);

  RaycastHit hit = raycast(world, voxelToWorld(3.5f, 3.5f, 3.5f), {0.0f, 25.0f, 0.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(3, 8, 3));
  CHECK(hit.distance == doctest::Approx(4.5f * VOXEL_SIZE));
}

TEST_CASE("raycast crosses a chunk border into negative coordinates")
{
  World world(0);
  addEmptyChunk(world, {0, 0, 0});
  setBlock(world, {3, -1, 3}, BLOCK_GRASS);

  RaycastHit hit = raycast(world, voxelToWorld(3.5f, 2.5f, 3.5f), {0.0f, -1.0f, 0.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(3, -1, 3));
  CHECK(hit.previousVoxel == glm::ivec3(3, 0, 3));
  CHECK(hit.distance == doctest::Approx(2.5f * VOXEL_SIZE));
}

TEST_CASE("raycast hits a block on the far side of a positive chunk border")
{
  World world(0);
  addEmptyChunk(world, {0, 0, 0});
  setBlock(world, {16, 2, 2}, BLOCK_DIRT);

  RaycastHit hit = raycast(world, voxelToWorld(13.5f, 2.5f, 2.5f), {1.0f, 0.0f, 0.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(16, 2, 2));
  CHECK(hit.previousVoxel == glm::ivec3(15, 2, 2));
  CHECK(hit.distance == doctest::Approx(2.5f * VOXEL_SIZE));
}

TEST_CASE("raycast follows a diagonal ray through the correct voxels")
{
  World world(0);
  setBlock(world, {1, 2, 0}, BLOCK_DIRT);
  setBlock(world, {2, 2, 0}, BLOCK_GRASS);

  RaycastHit hit = raycast(world, voxelToWorld(0.5f, 0.25f, 0.5f), {1.0f, 1.0f, 0.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(2, 2, 0));
  CHECK(hit.previousVoxel == glm::ivec3(2, 1, 0));
  CHECK(hit.distance == doctest::Approx(1.75f * std::sqrt(2.0f) * VOXEL_SIZE));
}

TEST_CASE("raycast follows a diagonal ray across chunk corners into negative coordinates")
{
  World world(0);
  addEmptyChunk(world, {0, 0, 0});
  setBlock(world, {-2, 4, -1}, BLOCK_DIRT);
  setBlock(world, {-2, 4, -2}, BLOCK_GRASS);

  RaycastHit hit = raycast(world, voxelToWorld(0.5f, 4.5f, 0.25f), {-1.0f, 0.0f, -1.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(-2, 4, -2));
  CHECK(hit.previousVoxel == glm::ivec3(-1, 4, -2));
  CHECK(hit.distance == doctest::Approx(1.5f * std::sqrt(2.0f) * VOXEL_SIZE));
}

TEST_CASE("raycast starting inside a solid voxel hits it immediately")
{
  World world(0);
  setBlock(world, {2, 2, 2}, BLOCK_DIRT);

  RaycastHit hit = raycast(world, voxelToWorld(2.5f, 2.5f, 2.5f), {0.0f, 0.0f, 1.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(2, 2, 2));
  CHECK(hit.previousVoxel == glm::ivec3(2, 2, 2));
  CHECK(hit.distance == doctest::Approx(0.0f));
}

TEST_CASE("raycast from an origin on a voxel boundary does not step early on a diagonal ray")
{
  World world(0);
  setBlock(world, {2, 0, 0}, BLOCK_DIRT);
  setBlock(world, {2, 1, 0}, BLOCK_GRASS);

  RaycastHit hit = raycast(world, voxelToWorld(1.0f, 0.5f, 0.5f), {1.0f, 1.0f, 0.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(2, 1, 0));
  CHECK(hit.previousVoxel == glm::ivec3(1, 1, 0));
  CHECK(hit.distance == doctest::Approx(std::sqrt(2.0f) * VOXEL_SIZE));
}

TEST_CASE("raycast from an origin on a voxel boundary reports the correct distance")
{
  World world(0);
  setBlock(world, {3, 0, 0}, BLOCK_DIRT);

  RaycastHit hit = raycast(world, voxelToWorld(1.0f, 0.5f, 0.5f), {1.0f, 0.0f, 0.0f}, 8.0f);

  REQUIRE(hit.hit);
  CHECK(hit.voxel == glm::ivec3(3, 0, 0));
  CHECK(hit.previousVoxel == glm::ivec3(2, 0, 0));
  CHECK(hit.distance == doctest::Approx(2.0f * VOXEL_SIZE));
}

TEST_CASE("raycast from an origin on a voxel boundary does not reach past the maximum distance")
{
  World world(0);
  setBlock(world, {4, 0, 0}, BLOCK_DIRT);

  RaycastHit hit = raycast(world, voxelToWorld(1.0f, 0.5f, 0.5f), {1.0f, 0.0f, 0.0f}, 2.5f);

  CHECK_FALSE(hit.hit);
}
