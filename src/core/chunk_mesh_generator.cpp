#include <core/chunk_mesh_generator.hpp>
#include <bit>
#include <cassert>
#include <core/block.hpp>

ChunkMeshGenerator::ChunkMeshGenerator(World &w) : world(w) {}

BinaryGrids ChunkMeshGenerator::getBinaryGrids(Chunk &chunk, uint32_t material)
{
  BinaryGrids grids{};

  const auto &chunkPos = chunk.pos;

  for (int x = 0; x < CHUNK_SIZE; x++)
  {
    for (int y = 0; y < CHUNK_SIZE; y++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        auto &voxel = chunk.getVoxel(x, y, z);

        if (voxel.type == BLOCK_AIR)
          continue;
        if (material != 0 && voxel.type != material)
          continue;

        grids.xy[x + 1][y + 1] |= (1u << (z + 1));
        grids.xz[x + 1][z + 1] |= (1u << (y + 1));
        grids.yz[y + 1][z + 1] |= (1u << (x + 1));
      }
    }
  }

  if (Chunk *east = world.getChunk(chunkPos.x + 1, chunkPos.y, chunkPos.z))
  {
    for (int y = 0; y < CHUNK_SIZE; y++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        if (east->getVoxel(0, y, z).type != BLOCK_AIR)
          grids.yz[y + 1][z + 1] |= (1u << (CHUNK_SIZE + 1));
      }
    }
  }

  if (Chunk *west = world.getChunk(chunkPos.x - 1, chunkPos.y, chunkPos.z))
  {
    for (int y = 0; y < CHUNK_SIZE; y++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        if (west->getVoxel(CHUNK_SIZE - 1, y, z).type != BLOCK_AIR)
          grids.yz[y + 1][z + 1] |= (1u << (0));
      }
    }
  }

  if (Chunk *top = world.getChunk(chunkPos.x, chunkPos.y + 1, chunkPos.z))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        if (top->getVoxel(x, 0, z).type != BLOCK_AIR)
          grids.xz[x + 1][z + 1] |= (1u << (CHUNK_SIZE + 1));
      }
    }
  }

  if (Chunk *top = world.getChunk(chunkPos.x, chunkPos.y - 1, chunkPos.z))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
      {
        if (top->getVoxel(x, CHUNK_SIZE - 1, z).type != BLOCK_AIR)
          grids.xz[x + 1][z + 1] |= (1u << (0));
      }
    }
  }

  if (Chunk *north = world.getChunk(chunkPos.x, chunkPos.y, chunkPos.z + 1))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int y = 0; y < CHUNK_SIZE; y++)
      {
        if (north->getVoxel(x, y, 0).type != BLOCK_AIR)
          grids.xy[x + 1][y + 1] |= (1u << (CHUNK_SIZE + 1));
      }
    }
  }

  if (Chunk *south = world.getChunk(chunkPos.x, chunkPos.y, chunkPos.z - 1))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int y = 0; y < CHUNK_SIZE; y++)
      {
        if (south->getVoxel(x, y, CHUNK_SIZE - 1).type != BLOCK_AIR)
          grids.xy[x + 1][y + 1] |= (1u << (0));
      }
    }
  }

  return grids;
}

BinaryGrid ChunkMeshGenerator::getAxisFaces(BinaryGrid &material, BinaryGrid &solid, bool positive)
{
  BinaryGrid axisFaces{};

  for (int i = 0; i < CHUNK_SIZE; i++)
  {
    for (int j = 0; j < CHUNK_SIZE; j++)
    {
      uint32_t row = material[i + 1][j + 1];
      uint32_t occupancy = solid[i + 1][j + 1];
      uint32_t shifted = positive ? occupancy >> 1 : occupancy << 1;
      axisFaces[i + 1][j + 1] = row & ~(shifted);
    }
  }

  return axisFaces;
}

BinaryGrid ChunkMeshGenerator::swizzleFaces(BinaryGrid &axisFaces)
{
  BinaryGrid swizzledFaces{};

  for (int i = 0; i < CHUNK_SIZE; i++)
  {
    for (int j = 0; j < CHUNK_SIZE; j++)
    {
      uint32_t row = axisFaces[i + 1][j + 1];

      while (row)
      {
        uint32_t k = std::countr_zero(row);
        swizzledFaces[i + 1][k] |= (1u << j);
        row &= row - 1;
      }
    }
  }

  return swizzledFaces;
}

void ChunkMeshGenerator::emitQuad(
    Axis axis,
    AxisDirection axisDirection,
    int slice,
    int row,
    int bit,
    int width,
    int height,
    glm::ivec3 &chunkPos,
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    int size, uint32_t material)
{
  glm::ivec3 origin = glm::ivec3(chunkPos.x, chunkPos.y, chunkPos.z) * CHUNK_SIZE;
  glm::vec3 v0, v1, v2, v3;
  glm::vec3 normal;

  switch (axis)
  {
  case Axis::X:
  {
    normal = glm::vec3(1, 0, 0);

    float x = origin.x + (axisDirection == AxisDirection::Positive ? slice + 1 : slice);

    v0 = {x, origin.y + row, origin.z + bit};
    v1 = {x, origin.y + row, origin.z + bit + height};
    v2 = {x, origin.y + row + width, origin.z + bit};
    v3 = {x, origin.y + row + width, origin.z + bit + height};
    break;
  }

  case Axis::Y:
  {
    normal = glm::vec3(0, 1, 0);

    float y = origin.y + (axisDirection == AxisDirection::Positive ? slice + 1 : slice);

    v0 = {origin.x + row, y, origin.z + bit};
    v1 = {origin.x + row + width, y, origin.z + bit};
    v2 = {origin.x + row, y, origin.z + bit + height};
    v3 = {origin.x + row + width, y, origin.z + bit + height};
    break;
  }

  case Axis::Z:
  {
    normal = glm::vec3(0, 0, 1);

    float z = origin.z + (axisDirection == AxisDirection::Positive ? slice + 1 : slice);

    v0 = {origin.x + row, origin.y + bit, z};
    v1 = {origin.x + row, origin.y + bit + height, z};
    v2 = {origin.x + row + width, origin.y + bit, z};
    v3 = {origin.x + row + width, origin.y + bit + height, z};
    break;
  }
  }

  if (axisDirection == AxisDirection::Negative)
  {
    normal *= -1;
  }

  assert(material < BLOCK_TYPE_COUNT);
  const BlockFaceTextures &faces = BLOCK_FACE_TEXTURES[material];
  uint32_t textureId = axis != Axis::Y                          ? faces.side
                       : axisDirection == AxisDirection::Positive ? faces.top
                                                                  : faces.bottom;

  uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

  glm::vec2 uv0;
  glm::vec2 uv1;
  glm::vec2 uv2;
  glm::vec2 uv3;
  if (axis == Axis::Y)
  {
    uv0 = {0.0f, 0.0f};
    uv1 = {(float)width, 0.0f};
    uv2 = {0.0f, (float)height};
    uv3 = {(float)width, (float)height};
  }
  else if (axis == Axis::X)
  {
    const float uMinZ = (axisDirection == AxisDirection::Positive) ? (float)height : 0.0f;
    const float uMaxZ = (axisDirection == AxisDirection::Positive) ? 0.0f : (float)height;
    uv0 = {uMinZ, (float)width};
    uv1 = {uMaxZ, (float)width};
    uv2 = {uMinZ, 0.0f};
    uv3 = {uMaxZ, 0.0f};
  }
  else
  {
    const float uMinX = (axisDirection == AxisDirection::Positive) ? 0.0f : (float)width;
    const float uMaxX = (axisDirection == AxisDirection::Positive) ? (float)width : 0.0f;
    uv0 = {uMinX, (float)height};
    uv1 = {uMinX, 0.0f};
    uv2 = {uMaxX, (float)height};
    uv3 = {uMaxX, 0.0f};
  }

  const float scale = (float)VOXEL_SIZE * (float)size;
  vertices.push_back({v0 * scale, uv0, normal, textureId});
  vertices.push_back({v1 * scale, uv1, normal, textureId});
  vertices.push_back({v2 * scale, uv2, normal, textureId});
  vertices.push_back({v3 * scale, uv3, normal, textureId});

  if (axisDirection == AxisDirection::Positive)
  {
    indices.insert(indices.end(), {baseVertex + 0, baseVertex + 2, baseVertex + 1,
                                   baseVertex + 1, baseVertex + 2, baseVertex + 3});
  }
  else
  {
    indices.insert(indices.end(), {baseVertex + 0, baseVertex + 1, baseVertex + 2,
                                   baseVertex + 1, baseVertex + 3, baseVertex + 2});
  }
}

void ChunkMeshGenerator::greedyMeshFaces(
    BinaryGrid &swizzledFaces,
    Axis axis,
    AxisDirection axisDirection,
    glm::ivec3 &chunkPos,
    std::vector<Vertex> &vertices,
    std::vector<uint32_t> &indices,
    int size, uint32_t material)
{
  for (int i = 0; i < CHUNK_SIZE; i++)
  {
    for (int j = 0; j < CHUNK_SIZE; j++)
    {
      uint32_t &row = swizzledFaces[i + 1][j + 1];

      while (row > 0)
      {
        uint32_t offset = std::countr_zero(row);
        uint32_t offsetedRow = row >> offset;
        uint32_t height = std::countr_one(offsetedRow);
        uint32_t mask = ((1u << height) - 1) << offset;

        uint32_t width = 1;
        for (int k = i + 1; k < CHUNK_SIZE; k++)
        {
          if ((swizzledFaces[k + 1][j + 1] & mask) != mask)
            break;
          width++;
          swizzledFaces[k + 1][j + 1] &= ~mask;
        }

        emitQuad(axis, axisDirection, j, i, offset, width, height, chunkPos, vertices, indices, size, material);

        row &= ~mask;
      }
    }
  }
}

ChunkMesh ChunkMeshGenerator::getChunkMesh(Chunk &chunk, int size)
{
  std::vector<Vertex> vertices{};
  std::vector<uint32_t> indices{};

  BinaryGrids solid = getBinaryGrids(chunk, 0);

  for (uint32_t material = BLOCK_AIR + 1; material < BLOCK_TYPE_COUNT; material++)
  {
    BinaryGrids occupancy = getBinaryGrids(chunk, material);

    std::array<FaceConfig, 6> faceConfigs = {{
        {Axis::X, AxisDirection::Positive, occupancy.yz},
        {Axis::X, AxisDirection::Negative, occupancy.yz},
        {Axis::Y, AxisDirection::Positive, occupancy.xz},
        {Axis::Y, AxisDirection::Negative, occupancy.xz},
        {Axis::Z, AxisDirection::Positive, occupancy.xy},
        {Axis::Z, AxisDirection::Negative, occupancy.xy},
    }};

    for (auto &config : faceConfigs)
    {
      BinaryGrid &solidPlane = (config.axis == Axis::X) ? solid.yz
                             : (config.axis == Axis::Y) ? solid.xz
                             : solid.xy;
      BinaryGrid axisFaces = getAxisFaces(config.occupancy, solidPlane, config.axisDirection == AxisDirection::Positive);
      BinaryGrid axisFacesSwizzled = swizzleFaces(axisFaces);
      greedyMeshFaces(axisFacesSwizzled, config.axis, config.axisDirection, chunk.pos, vertices, indices, size, material);
    }
  }

  return {vertices, indices};
}
