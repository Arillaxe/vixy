// The single-header implementations for tinygltf and stb live here (and ONLY
// here) so they are compiled exactly once across the whole project.
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <core/mesh.hpp>
#include <core/block.hpp>

#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>

tinygltf::Model Mesh::loadModel(const std::string &filename)
{
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

  if (!warn.empty())
  {
    std::cout << "glTF warning: " << warn << std::endl;
  }

  if (!err.empty())
  {
    std::cout << "glTF error: " << err << std::endl;
  }

  if (!ret)
  {
    throw std::runtime_error("Failed to load glTF model");
  }

  return model;
}

namespace
{
  float readComponent(const unsigned char *src, int componentType, bool normalized)
  {
    switch (componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
    {
      float value;
      std::memcpy(&value, src, sizeof(value));
      return value;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      if (normalized)
      {
        return static_cast<float>(*src) / 255.0f;
      }
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      if (normalized)
      {
        uint16_t value;
        std::memcpy(&value, src, sizeof(value));
        return static_cast<float>(value) / 65535.0f;
      }
      break;
    default:
      break;
    }

    throw std::runtime_error("Unsupported glTF attribute component type");
  }

  class AccessorReader
  {
  private:
    const unsigned char *data = nullptr;
    size_t stride = 0;
    size_t componentSize = 0;
    int componentType = 0;
    bool normalized = false;
    int numComponents = 0;
    size_t count = 0;

  public:
    AccessorReader(const tinygltf::Model &model, int accessorIndex, int expectedType)
    {
      if (accessorIndex < 0 || static_cast<size_t>(accessorIndex) >= model.accessors.size())
      {
        throw std::runtime_error("Invalid glTF accessor index");
      }

      const tinygltf::Accessor &accessor = model.accessors[accessorIndex];

      if (accessor.type != expectedType)
      {
        throw std::runtime_error("Unexpected glTF accessor type");
      }

      if (accessor.sparse.isSparse || accessor.bufferView < 0)
      {
        throw std::runtime_error("Sparse or bufferless glTF accessors are not supported");
      }

      if (static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size())
      {
        throw std::runtime_error("Invalid glTF buffer view index");
      }

      const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];

      if (bufferView.buffer < 0 || static_cast<size_t>(bufferView.buffer) >= model.buffers.size())
      {
        throw std::runtime_error("Invalid glTF buffer index");
      }

      const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

      int byteStride = accessor.ByteStride(bufferView);
      int size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
      int components = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));

      if (byteStride <= 0 || size <= 0 || components <= 0)
      {
        throw std::runtime_error("Invalid glTF accessor layout");
      }

      count = accessor.count;
      stride = static_cast<size_t>(byteStride);
      componentSize = static_cast<size_t>(size);
      componentType = accessor.componentType;
      normalized = accessor.normalized;
      numComponents = components;

      size_t elementSize = componentSize * static_cast<size_t>(numComponents);

      if (stride < elementSize)
      {
        throw std::runtime_error("glTF accessor stride is smaller than its element");
      }

      if (bufferView.byteOffset > buffer.data.size() || bufferView.byteLength > buffer.data.size() - bufferView.byteOffset)
      {
        throw std::runtime_error("glTF buffer view exceeds its buffer");
      }

      if (count > 0)
      {
        size_t viewLength = bufferView.byteLength;

        if (accessor.byteOffset > viewLength || elementSize > viewLength - accessor.byteOffset ||
            count - 1 > (viewLength - accessor.byteOffset - elementSize) / stride)
        {
          throw std::runtime_error("glTF accessor exceeds its buffer view");
        }

        data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
      }
    }

    size_t getCount() const { return count; }
    int getComponentType() const { return componentType; }

    void read(size_t index, float *out) const
    {
      const unsigned char *element = data + index * stride;

      for (int c = 0; c < numComponents; c++)
      {
        out[c] = readComponent(element + c * componentSize, componentType, normalized);
      }
    }
  };

  const int *findAttribute(const tinygltf::Primitive &primitive, const char *name)
  {
    auto it = primitive.attributes.find(name);
    return it == primitive.attributes.end() ? nullptr : &it->second;
  }
}

std::vector<Vertex> Mesh::calcVerticies()
{
  std::vector<Vertex> vertices;

  for (const auto &mesh : model.meshes)
  {
    for (const auto &primitive : mesh.primitives)
    {
      AccessorReader positions(model, primitive.attributes.at("POSITION"), TINYGLTF_TYPE_VEC3);

      if (positions.getComponentType() != TINYGLTF_COMPONENT_TYPE_FLOAT)
      {
        throw std::runtime_error("glTF POSITION must be float");
      }

      std::optional<AccessorReader> texCoords;
      std::optional<AccessorReader> normals;

      if (const int *index = findAttribute(primitive, "TEXCOORD_0"))
      {
        texCoords.emplace(model, *index, TINYGLTF_TYPE_VEC2);
      }

      if (const int *index = findAttribute(primitive, "NORMAL"))
      {
        normals.emplace(model, *index, TINYGLTF_TYPE_VEC3);

        if (normals->getComponentType() != TINYGLTF_COMPONENT_TYPE_FLOAT)
        {
          throw std::runtime_error("glTF NORMAL must be float");
        }
      }

      if ((texCoords && texCoords->getCount() != positions.getCount()) || (normals && normals->getCount() != positions.getCount()))
      {
        throw std::runtime_error("glTF attribute counts do not match");
      }

      vertices.reserve(vertices.size() + positions.getCount());

      for (size_t i = 0; i < positions.getCount(); i++)
      {
        Vertex vertex{};
        float values[3];

        positions.read(i, values);
        vertex.position = {values[0], values[1], values[2]};

        if (texCoords)
        {
          texCoords->read(i, values);
          vertex.texCoord = {values[0], values[1]};
        }
        else
        {
          vertex.texCoord = {0.0f, 0.0f};
        }

        if (normals)
        {
          normals->read(i, values);
          vertex.normal = {values[0], values[1], values[2]};
        }
        else
        {
          vertex.normal = {0.0f, 1.0f, 0.0f};
        }

        vertex.textureId = DEFAULT_MESH_TEXTURE;

        vertices.push_back(vertex);
      }
    }
  }

  return vertices;
}

std::vector<uint32_t> Mesh::calcIndices()
{
  std::vector<uint32_t> indices;
  uint32_t vertexOffset = 0;

  for (const auto &mesh : model.meshes)
  {
    for (const auto &primitive : mesh.primitives)
    {
      const tinygltf::Accessor &posAccessor = model.accessors[primitive.attributes.at("POSITION")];

      if (primitive.indices < 0)
      {
        indices.reserve(indices.size() + posAccessor.count);

        for (size_t i = 0; i < posAccessor.count; i++)
        {
          indices.push_back(vertexOffset + static_cast<uint32_t>(i));
        }

        vertexOffset += static_cast<uint32_t>(posAccessor.count);
        continue;
      }

      const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
      const tinygltf::BufferView &indexBufferView = model.bufferViews[indexAccessor.bufferView];
      const tinygltf::Buffer &indexBuffer = model.buffers[indexBufferView.buffer];

      const unsigned char *indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];
      size_t indexCount = indexAccessor.count;

      indices.reserve(indices.size() + indexCount);

      for (size_t i = 0; i < indexCount; i++)
      {
        uint32_t index = 0;

        switch (indexAccessor.componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
          index = *reinterpret_cast<const uint16_t *>(indexData + i * sizeof(uint16_t));
          break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
          index = *reinterpret_cast<const uint32_t *>(indexData + i * sizeof(uint32_t));
          break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
          index = *reinterpret_cast<const uint8_t *>(indexData + i * sizeof(uint8_t));
          break;

        default:
          throw std::runtime_error("Unsupported index component type");
          break;
        }

        indices.push_back(vertexOffset + index);
      }

      vertexOffset += static_cast<uint32_t>(posAccessor.count);
    }
  }

  return indices;
}

void Mesh::createVertexBuffer()
{
  if (vertices.empty())
  {
    return;
  }

  vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  auto stagingBuffer = vkResource.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc);
  auto stagingBufferMemory = vkResource.getBufferMemory(stagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);

  memcpy(dataStaging, vertices.data(), bufferSize);

  stagingBufferMemory.unmapMemory();

  vertexBuffer = vkResource.createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
  vertexBufferMemory = vkResource.getBufferMemory(vertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

  vkResource.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
}

void Mesh::createIndexBuffer()
{
  if (indices.empty())
  {
    return;
  }

  vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  auto stagingBuffer = vkResource.createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc);
  auto stagingBufferMemory = vkResource.getBufferMemory(stagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
  void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);

  memcpy(dataStaging, indices.data(), bufferSize);

  stagingBufferMemory.unmapMemory();

  indexBuffer = vkResource.createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
  indexBufferMemory = vkResource.getBufferMemory(indexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);

  vkResource.copyBuffer(stagingBuffer, indexBuffer, bufferSize);
}

Mesh::Mesh(VkResource &resource, const std::string &filename)
    : vkResource(resource),
      model(loadModel(filename)),
      vertices(calcVerticies()),
      indices(calcIndices())
{
  createVertexBuffer();
  createIndexBuffer();
}
