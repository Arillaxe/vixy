#ifndef CHUNK_MESH_GENERATOR_HPP
#define CHUNK_MESH_GENERATOR_HPP

#include <core/chunk_mesh.hpp>
#include <core/chunk.hpp>
#include <core/world.hpp>

using BinaryGrid = std::array<std::array<uint32_t, CHUNK_SIZE + 2>, CHUNK_SIZE + 2>;

enum Axis
{
  X,
  Y,
  Z
};

enum AxisDirection
{
  Positive,
  Negative
};

struct BinaryGrids
{
  BinaryGrid xy{};
  BinaryGrid xz{};
  BinaryGrid yz{};
};

struct FaceConfig
{
  Axis axis;
  AxisDirection axisDirection;
  BinaryGrid occupancy;
};

class ChunkMeshGenerator
{
  World &world;

  BinaryGrids getBinaryGrids(Chunk &chunk, uint32_t material); // material 0 = any solid
  BinaryGrid getAxisFaces(BinaryGrid &material, BinaryGrid &solid, bool positive);
  BinaryGrid swizzleFaces(BinaryGrid &axisFaces);
  void emitQuad(
      Axis axis,
      AxisDirection axisDirection,
      int slice,
      int row,
      int bit,
      int width,
      int height,
      std::vector<ChunkVertex> &vertices,
      std::vector<uint32_t> &indices,
      uint32_t material);
  void greedyMeshFaces(
      BinaryGrid &swizzledFaces,
      Axis axis,
      AxisDirection axisDirection,
      std::vector<ChunkVertex> &vertices,
      std::vector<uint32_t> &indices,
      uint32_t material);

public:
  ChunkMeshGenerator(World &w);

  ChunkMesh getChunkMesh(Chunk &chunk);
};

#endif // CHUNK_MESH_GENERATOR_HPP
