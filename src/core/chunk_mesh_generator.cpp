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
  else if (world.isImplicitSolid(chunkPos.x + 1, chunkPos.y, chunkPos.z))
  {
    for (int y = 0; y < CHUNK_SIZE; y++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
        grids.yz[y + 1][z + 1] |= (1u << (CHUNK_SIZE + 1));
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
  else if (world.isImplicitSolid(chunkPos.x - 1, chunkPos.y, chunkPos.z))
  {
    for (int y = 0; y < CHUNK_SIZE; y++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
        grids.yz[y + 1][z + 1] |= (1u << (0));
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
  else if (world.isImplicitSolid(chunkPos.x, chunkPos.y + 1, chunkPos.z))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
        grids.xz[x + 1][z + 1] |= (1u << (CHUNK_SIZE + 1));
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
  else if (world.isImplicitSolid(chunkPos.x, chunkPos.y - 1, chunkPos.z))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int z = 0; z < CHUNK_SIZE; z++)
        grids.xz[x + 1][z + 1] |= (1u << (0));
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
  else if (world.isImplicitSolid(chunkPos.x, chunkPos.y, chunkPos.z + 1))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int y = 0; y < CHUNK_SIZE; y++)
        grids.xy[x + 1][y + 1] |= (1u << (CHUNK_SIZE + 1));
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
  else if (world.isImplicitSolid(chunkPos.x, chunkPos.y, chunkPos.z - 1))
  {
    for (int x = 0; x < CHUNK_SIZE; x++)
    {
      for (int y = 0; y < CHUNK_SIZE; y++)
        grids.xy[x + 1][y + 1] |= (1u << (0));
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
    std::vector<ChunkVertex> &vertices,
    std::vector<uint32_t> &indices,
    uint32_t material)
{
  glm::ivec3 v0, v1, v2, v3;
  const bool positive = axisDirection == AxisDirection::Positive;

  switch (axis)
  {
  case Axis::X:
  {
    int x = positive ? slice + 1 : slice;

    v0 = {x, row, bit};
    v1 = {x, row, bit + height};
    v2 = {x, row + width, bit};
    v3 = {x, row + width, bit + height};
    break;
  }

  case Axis::Y:
  {
    int y = positive ? slice + 1 : slice;

    v0 = {row, y, bit};
    v1 = {row + width, y, bit};
    v2 = {row, y, bit + height};
    v3 = {row + width, y, bit + height};
    break;
  }

  case Axis::Z:
  {
    int z = positive ? slice + 1 : slice;

    v0 = {row, bit, z};
    v1 = {row, bit + height, z};
    v2 = {row + width, bit, z};
    v3 = {row + width, bit + height, z};
    break;
  }
  }

  uint32_t face = static_cast<uint32_t>(axis) * 2 + (positive ? 0 : 1);

  assert(material < BLOCK_TYPE_COUNT);
  const BlockFaceTextures &faces = BLOCK_FACE_TEXTURES[material];
  uint32_t textureId = axis != Axis::Y ? faces.side
                       : positive      ? faces.top
                                       : faces.bottom;

  uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

  glm::ivec2 uv0;
  glm::ivec2 uv1;
  glm::ivec2 uv2;
  glm::ivec2 uv3;
  if (axis == Axis::Y)
  {
    uv0 = {0, 0};
    uv1 = {width, 0};
    uv2 = {0, height};
    uv3 = {width, height};
  }
  else if (axis == Axis::X)
  {
    const int uMinZ = positive ? height : 0;
    const int uMaxZ = positive ? 0 : height;
    uv0 = {uMinZ, width};
    uv1 = {uMaxZ, width};
    uv2 = {uMinZ, 0};
    uv3 = {uMaxZ, 0};
  }
  else
  {
    const int uMinX = positive ? 0 : width;
    const int uMaxX = positive ? width : 0;
    uv0 = {uMinX, height};
    uv1 = {uMinX, 0};
    uv2 = {uMaxX, height};
    uv3 = {uMaxX, 0};
  }

  auto pushVertex = [&](const glm::ivec3 &p, const glm::ivec2 &uv)
  {
    assert(glm::all(glm::greaterThanEqual(p, glm::ivec3(0))) && glm::all(glm::lessThanEqual(p, glm::ivec3(CHUNK_SIZE))));
    assert(glm::all(glm::greaterThanEqual(uv, glm::ivec2(0))) && glm::all(glm::lessThanEqual(uv, glm::ivec2(CHUNK_SIZE))));
    vertices.push_back({
        ChunkVertex::pack(p.x, p.y, p.z, uv.x, uv.y, face),
        textureId,
    });
  };

  pushVertex(v0, uv0);
  pushVertex(v1, uv1);
  pushVertex(v2, uv2);
  pushVertex(v3, uv3);

  if (positive)
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
    std::vector<ChunkVertex> &vertices,
    std::vector<uint32_t> &indices,
    uint32_t material)
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

        emitQuad(axis, axisDirection, j, i, offset, width, height, vertices, indices, material);

        row &= ~mask;
      }
    }
  }
}

ChunkMesh ChunkMeshGenerator::getChunkMesh(Chunk &chunk)
{
  std::vector<ChunkVertex> vertices{};
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
      greedyMeshFaces(axisFacesSwizzled, config.axis, config.axisDirection, vertices, indices, material);
    }
  }

  return {vertices, indices};
}
