#ifndef GUI_HPP
#define GUI_HPP

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <core/window.hpp>
#include <renderer/gpu/gpu_context.hpp>
#include <renderer/gpu/gpu_resource.hpp>
#include <renderer/gpu/gpu_swapchain.hpp>

inline void check_vk_result(VkResult err)
{
  if (err == 0)
    return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0)
    abort();
}

class GUI
{
private:
  Window &window;
  GpuContext &gpuContext;
  GpuResource &gpuResource;
  GpuSwapchain &gpuSwapchain;

public:
  GUI(Window &win, GpuContext &context, GpuResource &resource, GpuSwapchain &swapchain)
      : window(win), gpuContext(context), gpuResource(resource), gpuSwapchain(swapchain)
  {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;
    io.DisplaySize.x = (float)swapchain.getExtent().width;
    io.DisplaySize.y = (float)swapchain.getExtent().height;

    ImGui::GetStyle().FontScaleMain = 1.5f;

    ImGui_ImplGlfw_InitForVulkan(window.getGLFWWindowHandle(), true);

    vk::Format colorFormat = swapchain.getSurfaceFormat().format;
    vk::Format depthFormat = swapchain.findDepthFormat();

    vk::PipelineRenderingCreateInfo renderingInfo{
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
    };

    ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
    pipelineInfo.PipelineRenderingCreateInfo = renderingInfo;
    pipelineInfo.RenderPass = nullptr;
    pipelineInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(gpuContext.getMsaaSamples());

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = *gpuContext.getInstance(),
        .PhysicalDevice = *gpuContext.getPhysicalDevice(),
        .Device = *gpuContext.getDevice(),
        .QueueFamily = gpuContext.getQueueFamilyIndex(),
        .Queue = *gpuContext.getQueue(),
        .DescriptorPool = *gpuResource.getDescriptorPool(),
        .MinImageCount = 2,
        .ImageCount = 2,
        .UseDynamicRendering = true,
        .CheckVkResultFn = check_vk_result,
    };

    initInfo.PipelineInfoMain = pipelineInfo;

    ImGui_ImplVulkan_Init(&initInfo);
  }

  ~GUI()
  {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  void beginFrame()
  {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }

  void endFrame(vk::raii::CommandBuffer &commandBuffer)
  {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer);
  }
};

#endif // GUI_HPP
