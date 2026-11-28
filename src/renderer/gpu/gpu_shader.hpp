#ifndef GPU_SHADER_HPP
#define GPU_SHADER_HPP

#include <renderer/vk.hpp>
#include <renderer/gpu/gpu_context.hpp>
#include <string>
#include <vector>

class GpuShader
{
private:
  GpuContext &gpuContext;
  vk::raii::ShaderModule shaderModule;

  vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const;

public:
  GpuShader(GpuContext &context, const std::string &filename);

  vk::raii::ShaderModule &getShaderModule() { return shaderModule; }
};

#endif // GPU_SHADER_HPP
