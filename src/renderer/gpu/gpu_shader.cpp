#include <renderer/gpu/gpu_shader.hpp>

#include <core/filesystem.hpp>

GpuShader::GpuShader(GpuContext &context, const std::string &filename)
    : gpuContext(context),
      shaderModule(createShaderModule(Filesystem::readFile(filename))) {}

vk::raii::ShaderModule GpuShader::createShaderModule(const std::vector<char> &code) const
{
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size() * sizeof(char),
      .pCode = reinterpret_cast<const uint32_t *>(code.data()),
  };
  vk::raii::ShaderModule shaderModule(gpuContext.getDevice(), createInfo);

  return shaderModule;
}
