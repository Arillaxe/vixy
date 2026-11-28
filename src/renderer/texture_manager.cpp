#include <renderer/texture_manager.hpp>
#include <stb_image.h>

TextureManager::TextureManager(GpuResource &resource) : gpuResource(resource) {}

Texture &TextureManager::loadTexture(const std::string &filename)
{
  int width, height, channels;

  stbi_uc *data = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);

  if (!data)
  {
    throw std::runtime_error("Failed to load image: " + filename);
  }

  const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;

  vk::raii::Image image = gpuResource.createImage(
      width,
      height,
      1,
      vk::SampleCountFlagBits::e1,
      vk::Format::eR8G8B8A8Srgb,
      vk::ImageTiling::eOptimal,
      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled);
  vk::raii::DeviceMemory memory = gpuResource.getImageMemory(image, vk::MemoryPropertyFlagBits::eDeviceLocal);

  vk::raii::Buffer stagingBuffer = gpuResource.createBuffer(size, vk::BufferUsageFlagBits::eTransferSrc);
  vk::raii::DeviceMemory stagingMemory = gpuResource.getBufferMemory(
      stagingBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

  void *mapped = stagingMemory.mapMemory(0, size);

  memcpy(mapped, data, size);

  stagingMemory.unmapMemory();

  gpuResource.uploadImage(stagingBuffer, image, width, height);

  vk::raii::ImageView view = gpuResource.createImageView(image, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, 1);

  stbi_image_free(data);

  uint32_t id = static_cast<uint32_t>(textures.size());

  return textures.emplace_back(Texture{
      std::move(image),
      std::move(memory),
      std::move(view),
      id,
  });
}
