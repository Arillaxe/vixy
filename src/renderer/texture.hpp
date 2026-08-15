#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <renderer/vk.hpp>

struct Texture
{
  vk::raii::Image image;
  vk::raii::DeviceMemory memory;
  vk::raii::ImageView view;

  uint32_t id;
};

#endif // TEXTURE_HPP
