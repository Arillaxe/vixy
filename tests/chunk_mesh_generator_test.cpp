#include <doctest.h>
#include <core/chunk_mesh_generator.hpp>
#include <world_fixture.hpp>

namespace
{
enum Face : uint32_t
{
  FACE_POS_X = 0,
  FACE_NEG_X = 1,
  FACE_POS_Y = 2,
  FACE_NEG_Y = 3,
  FACE_POS_Z = 4,
  FACE_NEG_Z = 5,
};

struct DecodedVertex
{
  glm::ivec3 position;
  glm::ivec2 uv;
  uint32_t face;
  uint32_t textureId;
};

DecodedVertex decode(const ChunkVertex &vertex)
{
  const uint32_t d = vertex.data;
  return {
      {static_cast<int>(d & 31u), static_cast<int>((d >> 5) & 31u), static_cast<int>((d >> 10) & 31u)},
      {static_cast<int>((d >> 15) & 31u), static_cast<int>((d >> 20) & 31u)},
      (d >> 25) & 7u,
      vertex.textureId,
  };
}

struct Quad
{
  uint32_t face;
  uint32_t textureId;
  glm::ivec3 min;
  glm::ivec3 max;

  int area() const
  {
    glm::ivec3 size = max - min;
    int result = 1;
    for (int i = 0; i < 3; i++)
      if (size[i] != 0)
        result *= size[i];
    return result;
  }
};

std::vector<Quad> getQuads(const ChunkMesh &mesh)
{
  REQUIRE(mesh.vertices.size() % 4 == 0);
  REQUIRE(mesh.indices.size() == mesh.vertices.size() / 4 * 6);

  for (uint32_t index : mesh.indices)
    REQUIRE(index < mesh.vertices.size());

  std::vector<Quad> quads;

  for (size_t i = 0; i < mesh.vertices.size(); i += 4)
  {
    DecodedVertex first = decode(mesh.vertices[i]);
    Quad quad{first.face, first.textureId, first.position, first.position};

    for (size_t j = 1; j < 4; j++)
    {
      DecodedVertex v = decode(mesh.vertices[i + j]);
      REQUIRE(v.face == quad.face);
      REQUIRE(v.textureId == quad.textureId);
      quad.min = glm::min(quad.min, v.position);
      quad.max = glm::max(quad.max, v.position);
    }

    const int flatAxis = static_cast<int>(quad.face / 2);
    REQUIRE(quad.min[flatAxis] == quad.max[flatAxis]);

    quads.push_back(quad);
  }

  for (size_t q = 0; q < quads.size(); q++)
  {
    const uint32_t *idx = &mesh.indices[q * 6];
    const uint32_t base = static_cast<uint32_t>(q * 4);
    for (int k = 0; k < 6; k++)
      REQUIRE((idx[k] >= base && idx[k] < base + 4));
  }

  return quads;
}

int countFaces(const std::vector<Quad> &quads, uint32_t face)
{
  int count = 0;
  for (const Quad &quad : quads)
    if (quad.face == face)
      count++;
  return count;
}

int faceArea(const std::vector<Quad> &quads, uint32_t face)
{
  int area = 0;
  for (const Quad &quad : quads)
    if (quad.face == face)
      area += quad.area();
  return area;
}

void fillBox(Chunk &chunk, glm::ivec3 min, glm::ivec3 max, BlockType type)
{
  for (int x = min.x; x < max.x; x++)
    for (int y = min.y; y < max.y; y++)
      for (int z = min.z; z < max.z; z++)
        chunk.getVoxel(x, y, z).type = static_cast<uint8_t>(type);
}

void fillChunk(Chunk &chunk, BlockType type)
{
  fillBox(chunk, {0, 0, 0}, {CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE}, type);
}
}

TEST_CASE("an empty chunk produces no mesh")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  ChunkMeshGenerator generator(world);

  ChunkMesh mesh = generator.getChunkMesh(chunk);

  CHECK(mesh.vertices.empty());
  CHECK(mesh.indices.empty());
}

TEST_CASE("a single voxel produces six unit quads")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  chunk.getVoxel(5, 6, 7).type = BLOCK_GRASS;
  ChunkMeshGenerator generator(world);

  ChunkMesh mesh = generator.getChunkMesh(chunk);
  std::vector<Quad> quads = getQuads(mesh);

  REQUIRE(quads.size() == 6);
  CHECK(mesh.vertices.size() == 24);
  CHECK(mesh.indices.size() == 36);

  for (uint32_t face = 0; face < 6; face++)
    CHECK(countFaces(quads, face) == 1);

  for (const Quad &quad : quads)
  {
    CHECK(quad.area() == 1);
    CHECK(glm::all(glm::greaterThanEqual(quad.min, glm::ivec3(5, 6, 7))));
    CHECK(glm::all(glm::lessThanEqual(quad.max, glm::ivec3(6, 7, 8))));

    const int flatAxis = static_cast<int>(quad.face / 2);
    const bool positive = quad.face % 2 == 0;
    const glm::ivec3 voxel(5, 6, 7);
    CHECK(quad.min[flatAxis] == voxel[flatAxis] + (positive ? 1 : 0));
  }
}

TEST_CASE("faces use the block's top, bottom and side textures")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  chunk.getVoxel(1, 1, 1).type = BLOCK_GRASS;
  ChunkMeshGenerator generator(world);

  std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));
  REQUIRE(quads.size() == 6);

  const BlockFaceTextures &textures = BLOCK_FACE_TEXTURES[BLOCK_GRASS];

  for (const Quad &quad : quads)
  {
    CAPTURE(quad.face);
    if (quad.face == FACE_POS_Y)
      CHECK(quad.textureId == textures.top);
    else if (quad.face == FACE_NEG_Y)
      CHECK(quad.textureId == textures.bottom);
    else
      CHECK(quad.textureId == textures.side);
  }
}

TEST_CASE("a full chunk is merged into one quad per face")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  fillChunk(chunk, BLOCK_DIRT);
  ChunkMeshGenerator generator(world);

  std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));

  REQUIRE(quads.size() == 6);
  for (uint32_t face = 0; face < 6; face++)
  {
    CAPTURE(face);
    CHECK(countFaces(quads, face) == 1);
    CHECK(faceArea(quads, face) == CHUNK_SIZE * CHUNK_SIZE);
  }
}

TEST_CASE("a one-voxel slab is merged into one quad per face")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  fillBox(chunk, {0, 0, 0}, {CHUNK_SIZE, 1, CHUNK_SIZE}, BLOCK_DIRT);
  ChunkMeshGenerator generator(world);

  std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));

  REQUIRE(quads.size() == 6);
  CHECK(faceArea(quads, FACE_POS_Y) == CHUNK_SIZE * CHUNK_SIZE);
  CHECK(faceArea(quads, FACE_NEG_Y) == CHUNK_SIZE * CHUNK_SIZE);
  CHECK(faceArea(quads, FACE_POS_X) == CHUNK_SIZE);
  CHECK(faceArea(quads, FACE_NEG_X) == CHUNK_SIZE);
  CHECK(faceArea(quads, FACE_POS_Z) == CHUNK_SIZE);
  CHECK(faceArea(quads, FACE_NEG_Z) == CHUNK_SIZE);

  for (const Quad &quad : quads)
  {
    if (quad.face == FACE_POS_Y)
      CHECK(quad.min.y == 1);
    if (quad.face == FACE_NEG_Y)
      CHECK(quad.min.y == 0);
  }
}

TEST_CASE("faces between two solid voxels are culled")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  ChunkMeshGenerator generator(world);

  SUBCASE("same material merges into a 1x1x2 box")
  {
    chunk.getVoxel(4, 4, 4).type = BLOCK_DIRT;
    chunk.getVoxel(4, 5, 4).type = BLOCK_DIRT;

    std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));

    CHECK(quads.size() == 6);
    CHECK(faceArea(quads, FACE_POS_X) == 2);
    CHECK(faceArea(quads, FACE_POS_Y) == 1);
  }

  SUBCASE("different materials still cull the shared face")
  {
    chunk.getVoxel(4, 4, 4).type = BLOCK_DIRT;
    chunk.getVoxel(4, 5, 4).type = BLOCK_GRASS;

    std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));

    CHECK(quads.size() == 10);
    CHECK(countFaces(quads, FACE_POS_Y) == 1);
    CHECK(countFaces(quads, FACE_NEG_Y) == 1);
    CHECK(faceArea(quads, FACE_POS_X) == 2);
  }
}

TEST_CASE("a solid neighbour chunk culls the shared border face")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  fillChunk(chunk, BLOCK_DIRT);

  struct Neighbour
  {
    glm::ivec3 offset;
    uint32_t culledFace;
  };

  const Neighbour neighbours[] = {
      {{1, 0, 0}, FACE_POS_X},
      {{-1, 0, 0}, FACE_NEG_X},
      {{0, 1, 0}, FACE_POS_Y},
      {{0, -1, 0}, FACE_NEG_Y},
      {{0, 0, 1}, FACE_POS_Z},
      {{0, 0, -1}, FACE_NEG_Z},
  };

  for (const Neighbour &neighbour : neighbours)
  {
    CAPTURE(neighbour.culledFace);

    Chunk &other = addEmptyChunk(world, neighbour.offset);
    fillChunk(other, BLOCK_GRASS);

    ChunkMeshGenerator generator(world);
    std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));

    CHECK(quads.size() == 5);
    CHECK(countFaces(quads, neighbour.culledFace) == 0);

    world.getChunks().erase(neighbour.offset);
  }
}

TEST_CASE("an empty neighbour chunk does not cull the border face")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  fillChunk(chunk, BLOCK_DIRT);
  addEmptyChunk(world, {1, 0, 0});
  ChunkMeshGenerator generator(world);

  std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));

  CHECK(quads.size() == 6);
  CHECK(faceArea(quads, FACE_POS_X) == CHUNK_SIZE * CHUNK_SIZE);
}

TEST_CASE("only the touching voxels of a neighbour chunk cull border faces")
{
  World world(0);
  Chunk &chunk = addEmptyChunk(world, {0, 0, 0});
  fillChunk(chunk, BLOCK_DIRT);

  Chunk &west = addEmptyChunk(world, {-1, 0, 0});
  west.getVoxel(CHUNK_SIZE - 1, 3, 4).type = BLOCK_DIRT;
  west.getVoxel(CHUNK_SIZE - 2, 7, 7).type = BLOCK_DIRT;

  ChunkMeshGenerator generator(world);
  std::vector<Quad> quads = getQuads(generator.getChunkMesh(chunk));

  CHECK(faceArea(quads, FACE_NEG_X) == CHUNK_SIZE * CHUNK_SIZE - 1);
  CHECK(faceArea(quads, FACE_POS_X) == CHUNK_SIZE * CHUNK_SIZE);
}

TEST_CASE("a chunk's mesh does not depend on its world position")
{
  auto buildMesh = [](glm::ivec3 pos)
  {
    World world(0);
    Chunk &chunk = addEmptyChunk(world, pos);
    chunk.getVoxel(0, 0, 0).type = BLOCK_DIRT;
    chunk.getVoxel(CHUNK_SIZE - 1, CHUNK_SIZE - 1, CHUNK_SIZE - 1).type = BLOCK_GRASS;
    fillBox(chunk, {4, 2, 6}, {9, 5, 8}, BLOCK_DIRT);
    ChunkMeshGenerator generator(world);
    return generator.getChunkMesh(chunk);
  };

  ChunkMesh atOrigin = buildMesh({0, 0, 0});
  ChunkMesh elsewhere = buildMesh({-3, -2, -5});

  REQUIRE(getQuads(atOrigin).size() == 18);
  REQUIRE(atOrigin.vertices.size() == elsewhere.vertices.size());
  for (size_t i = 0; i < atOrigin.vertices.size(); i++)
  {
    CAPTURE(i);
    CHECK(atOrigin.vertices[i].data == elsewhere.vertices[i].data);
    CHECK(atOrigin.vertices[i].textureId == elsewhere.vertices[i].textureId);
  }
  CHECK(atOrigin.indices == elsewhere.indices);
}
