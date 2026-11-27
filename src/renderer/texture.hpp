#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <renderer/vk.hpp>

constexpr uint32_t MAX_TEXTURES = 4096;

struct Texture
{
  vk::raii::Image image;
  vk::raii::DeviceMemory memory;
  vk::raii::ImageView view;

  uint32_t id;
};

#endif // TEXTURE_HPP
