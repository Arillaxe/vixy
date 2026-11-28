#ifndef BLOCK_HPP
#define BLOCK_HPP

#include <array>
#include <cstdint>

enum BlockTexture : uint32_t
{
  BLOCK_TEXTURE_GRASS = 0,
  BLOCK_TEXTURE_DIRT = 1,
  BLOCK_TEXTURE_DIRT_GRASS = 2,
  BLOCK_TEXTURE_COUNT
};

constexpr std::array<const char *, BLOCK_TEXTURE_COUNT> BLOCK_TEXTURE_PATHS = {
    "textures/grass.png",
    "textures/dirt.png",
    "textures/dirt_grass.png",
};

enum BlockType : uint32_t
{
  BLOCK_AIR = 0,
  BLOCK_GRASS = 1,
  BLOCK_DIRT = 2,
  BLOCK_TYPE_COUNT
};

struct BlockFaceTextures
{
  BlockTexture top;
  BlockTexture bottom;
  BlockTexture side;
};

static_assert(BLOCK_TYPE_COUNT <= 256, "BlockType must fit in Voxel::type");

constexpr std::array<BlockFaceTextures, BLOCK_TYPE_COUNT> BLOCK_FACE_TEXTURES = {{
    {BLOCK_TEXTURE_GRASS, BLOCK_TEXTURE_GRASS, BLOCK_TEXTURE_GRASS},
    {BLOCK_TEXTURE_GRASS, BLOCK_TEXTURE_DIRT, BLOCK_TEXTURE_DIRT_GRASS},
    {BLOCK_TEXTURE_DIRT, BLOCK_TEXTURE_DIRT, BLOCK_TEXTURE_DIRT},
}};

constexpr BlockTexture DEFAULT_MESH_TEXTURE = BLOCK_TEXTURE_GRASS;

#endif // BLOCK_HPP
