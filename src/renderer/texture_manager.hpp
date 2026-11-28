#ifndef TEXTURE_MANAGER_HPP
#define TEXTURE_MANAGER_HPP

#include <renderer/texture.hpp>
#include <vector>
#include <renderer/gpu/gpu_resource.hpp>

struct LoadedImage
{
  int width;
  int height;
  std::vector<std::byte> pixels;
};

class TextureManager
{
private:
  GpuResource &gpuResource;
  std::vector<Texture> textures;

public:
  TextureManager(GpuResource &resource);

  Texture &loadTexture(const std::string &filename);
};

#endif // TEXTURE_MANAGER_HPP
