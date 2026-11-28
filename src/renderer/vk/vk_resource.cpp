#include <renderer/vk/vk_resource.hpp>

#include <array>
#include <stdexcept>
#include <iostream>
#include <imgui/imgui_impl_vulkan.h>
#include <renderer/texture.hpp>

VkResource::VkResource(VkContext &context, VkCommand &command)
    : vkContext(context),
      vkCommand(command),
      descriptorPool(createDescriptorPool()) {}

uint32_t VkResource::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
  vk::PhysicalDeviceMemoryProperties memProperties =
      vkContext
          .getPhysicalDevice()
          .getMemoryProperties();

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
  {
    if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
    {
      return i;
    }
  }

  throw std::runtime_error("failed to fuind suitable memory type!");
}

vk::raii::DescriptorPool VkResource::createDescriptorPool()
{
  std::array<vk::DescriptorPoolSize, 4> poolSize{{
      {
          .type = vk::DescriptorType::eUniformBufferDynamic,
          .descriptorCount = 1,
      },
      {
          .type = vk::DescriptorType::eSampledImage,
          .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE,
      },
      {
          .type = vk::DescriptorType::eSampler,
          .descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE,
      },
      {
          .type = vk::DescriptorType::eCombinedImageSampler,
          .descriptorCount = MAX_TEXTURES,
      },
  }};
  vk::DescriptorPoolCreateInfo poolInfo{
      .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
      .maxSets = 100,
      .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
      .pPoolSizes = poolSize.data(),
  };

  return vk::raii::DescriptorPool(vkContext.getDevice(), poolInfo);
}

vk::raii::ImageView VkResource::createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlagBits aspectFlags, uint32_t mipLevels) const
{
  vk::ImageViewCreateInfo viewInfo{
      .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1},
  };

  return vk::raii::ImageView(vkContext.getDevice(), viewInfo);
}

vk::raii::Buffer VkResource::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, bool shared)
{
  // static uint32_t counter = 0;
  // counter++;
  // std::cout << counter << std::endl;

  std::array<uint32_t, 2> queueFamilyIndices{
      vkContext.getQueueFamilyIndex(),
      vkContext.getTransferQueueFamilyIndex(),
  };

  shared = shared && vkContext.hasSeparateTransferFamily();

  vk::BufferCreateInfo bufferInfo{
      .size = size,
      .usage = usage,
      .sharingMode = shared ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
      .queueFamilyIndexCount = shared ? static_cast<uint32_t>(queueFamilyIndices.size()) : 0u,
      .pQueueFamilyIndices = shared ? queueFamilyIndices.data() : nullptr,
  };

  return vk::raii::Buffer(vkContext.getDevice(), bufferInfo);
}

vk::raii::DeviceMemory VkResource::getBufferMemory(vk::raii::Buffer &buffer, vk::MemoryPropertyFlags properties)
{
  vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
  vk::MemoryAllocateInfo memoryAllocateInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties),
  };
  vk::raii::DeviceMemory deviceMemory = vk::raii::DeviceMemory(vkContext.getDevice(), memoryAllocateInfo);
  buffer.bindMemory(*deviceMemory, 0);

  return deviceMemory;
}

vk::raii::DeviceMemory VkResource::getImageMemory(vk::raii::Image &image, vk::MemoryPropertyFlags properties)
{
  vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
  vk::MemoryAllocateInfo memoryAllocateInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties),
  };
  vk::raii::DeviceMemory deviceMemory = vk::raii::DeviceMemory(vkContext.getDevice(), memoryAllocateInfo);
  image.bindMemory(*deviceMemory, 0);

  return deviceMemory;
}

void VkResource::copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
{
  copyBufferEx(srcBuffer, dstBuffer, vk::BufferCopy{.size = size});
}

void VkResource::copyBufferEx(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::BufferCopy copyParams)
{
  vkCommand.submitSingleTimeCommands([&](vk::raii::CommandBuffer &commandBuffer)
                                     { commandBuffer.copyBuffer(*srcBuffer, *dstBuffer, copyParams); });
}

vk::raii::Image VkResource::createImage(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevels,
    vk::SampleCountFlagBits numSamples,
    vk::Format format,
    vk::ImageTiling tiling,
    vk::ImageUsageFlags usage)
{
  vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent = {width, height, 1},
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .samples = numSamples,
      .tiling = tiling,
      .usage = usage,
      .sharingMode = vk::SharingMode::eExclusive,
  };

  return vk::raii::Image(vkContext.getDevice(), imageInfo);
}

void VkResource::transition_image_layout(
    vk::raii::CommandBuffer &commandBuffer,
    vk::Image image,
    vk::ImageLayout old_layout,
    vk::ImageLayout new_layout,
    vk::AccessFlags2 src_access_mask,
    vk::AccessFlags2 dst_access_mask,
    vk::PipelineStageFlags2 src_stage_mask,
    vk::PipelineStageFlags2 dst_stage_mask,
    vk::ImageAspectFlags image_aspect_flags)
{
  vk::ImageMemoryBarrier2 barrier = {
      .srcStageMask = src_stage_mask,
      .srcAccessMask = src_access_mask,
      .dstStageMask = dst_stage_mask,
      .dstAccessMask = dst_access_mask,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = {
          .aspectMask = image_aspect_flags,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };

  vk::DependencyInfo dependency_info = {
      .dependencyFlags = {},
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &barrier,
  };

  commandBuffer.pipelineBarrier2(dependency_info);
}

void VkResource::uploadImage(vk::raii::Buffer &stagingBuffer, vk::raii::Image &image, int width, int height)
{
  auto record = [&](vk::raii::CommandBuffer &commandBuffer)
  {
    transition_image_layout(
        commandBuffer,
        image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eTransferDstOptimal,
        {},
        vk::AccessFlagBits2::eTransferWrite,
        vk::PipelineStageFlagBits2::eTopOfPipe,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::ImageAspectFlagBits::eColor);

    vk::BufferImageCopy region{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,

        .imageSubresource = {
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },

        .imageOffset = {0, 0, 0},

        .imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
    };

    commandBuffer.copyBufferToImage(
        *stagingBuffer,
        *image,
        vk::ImageLayout::eTransferDstOptimal,
        region);

    transition_image_layout(
        commandBuffer,
        image,
        vk::ImageLayout::eTransferDstOptimal,
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::AccessFlagBits2::eTransferWrite,
        vk::AccessFlagBits2::eShaderSampledRead,
        vk::PipelineStageFlagBits2::eTransfer,
        vk::PipelineStageFlagBits2::eFragmentShader,
        vk::ImageAspectFlagBits::eColor);
  };

  vkCommand.submitSingleTimeCommands(record, true);
}
