#include <renderer/renderer.hpp>

#include <ecs/components/mesh_component.hpp>
#include <ecs/components/transform_component.hpp>
#include <renderer/push.hpp>
#include <cassert>
#include <core/chunk.hpp>
#include <iostream>
#include <algorithm>
#include <core/frustum_culler.hpp>
#include <renderer/mesh_allocator.hpp>

constexpr int MAX_FRAMES_IN_FLIGHT = 2;
constexpr int MAX_DRAW_SIZE = sizeof(vk::DrawIndirectCommand) * 100000;

Renderer::Renderer(Window &win, Scene &_scene, Camera &cam, ThreadQueue<MeshAllocation> &lQueue, ThreadQueue<glm::ivec3> &uQueue)
		: window(win),
			vkContext(win),
			vkCommand(vkContext),
			vkResource(vkContext, vkCommand),
			vkSwapchain(vkContext, win, vkResource, vkSynchronization),
			scene(_scene),
			commandBuffers(vkCommand.createCommandBuffers(MAX_FRAMES_IN_FLIGHT)),
			mainPipeline(vkContext, vkSwapchain, vkResource),
			camera(cam),
			queryPool(
					vkContext.getDevice(),
					vk::QueryPoolCreateInfo{}
							.setQueryType(vk::QueryType::eTimestamp)
							.setQueryCount(MAX_FRAMES_IN_FLIGHT * 2)),
			loadQueue(lQueue),
			unloadQueue(uQueue),
			gui(window, vkContext, vkResource, vkSwapchain),
			vertexBuffer(vkResource.createBuffer(VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst)),
			vertexBufferMemory(vkResource.getBufferMemory(vertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			indexBuffer(vkResource.createBuffer(INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst)),
			indexBufferMemory(vkResource.getBufferMemory(indexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			indirectBuffer(vkResource.createBuffer(MAX_DRAW_SIZE, vk::BufferUsageFlagBits::eIndirectBuffer)),
			indirectBufferMemory(vkResource.getBufferMemory(indirectBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)),
			textureManager(vkResource),
			sampler(createSampler())
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkSynchronization.emplace_back(vkContext);
	}

	loadTexture("textures/grass.png");
	loadTexture("textures/dirt.png");
	loadTexture("textures/dirt_grass.png");
}

void Renderer::loadTexture(const std::string &filename)
{
	auto &texture = textureManager.loadTexture(filename);
	auto &descriptorSet = mainPipeline.getDescriptorSet();

	vk::DescriptorImageInfo descriptorInfo{
			.sampler = *sampler,
			.imageView = *texture.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	};

	vk::WriteDescriptorSet write{
			.dstSet = *descriptorSet,
			.dstBinding = 1,
			.dstArrayElement = texture.id,
			.descriptorCount = 1,
			.descriptorType =
					vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &descriptorInfo,
	};

	vkContext.getDevice().updateDescriptorSets(write, {});
}

vk::raii::Sampler Renderer::createSampler()
{
	vk::SamplerCreateInfo samplerInfo{
			.magFilter = vk::Filter::eNearest,
			.minFilter = vk::Filter::eNearest,

			.mipmapMode = vk::SamplerMipmapMode::eNearest,

			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,

			.mipLodBias = 0.0f,

			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 1.0f,

			.compareEnable = VK_FALSE,

			.minLod = 0.0f,
			.maxLod = VK_LOD_CLAMP_NONE,

			.borderColor = vk::BorderColor::eIntOpaqueBlack,

			.unnormalizedCoordinates = VK_FALSE,
	};

	return vk::raii::Sampler{
			vkContext.getDevice(),
			samplerInfo,
	};
}

void Renderer::transition_image_layout(
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

void Renderer::render()
{
	static bool first[MAX_FRAMES_IN_FLIGHT] = {true, true};
	auto start = std::chrono::high_resolution_clock::now();

	vkSynchronization[frameIndex].waitDrawFence();
	std::array<uint64_t, 2> timestamps{};

	if (!first[frameIndex])
	{
		auto queryPoolResult = (*vkContext.getDevice()).getQueryPoolResults(*queryPool, frameIndex * 2, 2, sizeof(timestamps), timestamps.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64);

		gpuMs =
				(timestamps[1] - timestamps[0]) *
				vkContext.getPhysicalDevice().getProperties().limits.timestampPeriod /
				1'000'000.0;
	}

	uint32_t imageIndex = vkSwapchain.acquireNextImage(frameIndex);
	auto &commandBuffer = commandBuffers[frameIndex];

	commandBuffer.reset();

	auto &entities = scene.getEntities();

	commandBuffer.begin({});

	commandBuffer.resetQueryPool(*queryPool, frameIndex * 2, 2);
	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, frameIndex * 2);

	transition_image_layout(
			commandBuffer,
			vkSwapchain.getImages()[imageIndex],
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor);

	transition_image_layout(
			commandBuffer,
			*vkSwapchain.getColorImage(),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor);

	transition_image_layout(
			commandBuffer,
			*vkSwapchain.getDepthImage(),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

	vk::ClearValue clearColor = vk::ClearColorValue(0.49f, 0.70f, 1.00f, 1.0f);
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

	vk::RenderingAttachmentInfo attachmentInfo = {
			.imageView = vkSwapchain.getColorImageView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.resolveMode = vk::ResolveModeFlagBits::eAverage,
			.resolveImageView = vkSwapchain.getImageViews()[imageIndex],
			.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor,
	};

	vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = vkSwapchain.getDepthImageView(),
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth,
	};

	vk::RenderingInfo renderingInfo = {
			.renderArea = {.offset = {0, 0}, .extent = vkSwapchain.getExtent()},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo,
	};

	commandBuffer.beginRendering(renderingInfo);

	vk::Viewport viewport{
			0.0f,
			0.0f,
			static_cast<float>(vkSwapchain.getExtent().width),
			static_cast<float>(vkSwapchain.getExtent().height),
			0.0f,
			1.0f,
	};
	vk::Rect2D scissor{vk::Offset2D{0, 0}, vkSwapchain.getExtent()};

	commandBuffer.setViewport(0, viewport);
	commandBuffer.setScissor(0, scissor);
	commandBuffer.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline.getPipeline());
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mainPipeline.getPipelineLayout(), 0, *mainPipeline.getDescriptorSet(), nullptr);

	for (auto &entity : entities)
	{
		if (!entity.isActive())
			continue;

		auto meshComponent = entity.getComponent<MeshComponent>();
		auto transformComponent = entity.getComponent<TransformComponent>();

		if (!meshComponent || !transformComponent)
			continue;

		auto &mesh = meshComponent->getMesh();
		auto &indices = mesh.getIndices();

		commandBuffer.bindVertexBuffers(0, *mesh.getVertexBuffer(), {0});
		commandBuffer.bindIndexBuffer(*mesh.getIndexBuffer(), 0, vk::IndexTypeValue<uint32_t>::value);
		PushConstants pushConstants;

		pushConstants.model = transformComponent->getTransformMatrix();

		commandBuffer.pushConstants<PushConstants>(mainPipeline.getPipelineLayout(), vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
		commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
	}

	PushConstants pushConstants;
	pushConstants.model = glm::mat4(1.0f);

	unloadQueueSize = unloadQueue.getSize();

	while (unloadQueue.tryPop(toUnload))
	{
		chunkMeshesAllocations.erase(toUnload);
	}

	loadQueueSize = loadQueue.getSize();

	while (loadQueue.tryPop(toLoad))
	{
		chunkMeshesAllocations.insert_or_assign(toLoad.pos, std::move(toLoad));
	}

	totalIndices = 0;

	float aspectRatio = static_cast<float>(vkSwapchain.getExtent().width) / static_cast<float>(vkSwapchain.getExtent().height);
	FrustumCuller culler(camera, aspectRatio);

	commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
	commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexTypeValue<uint32_t>::value);
	commandBuffer.pushConstants<PushConstants>(mainPipeline.getPipelineLayout(), vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
	auto indirectCommands = static_cast<vk::DrawIndexedIndirectCommand *>(indirectBufferMemory.mapMemory(0, MAX_DRAW_SIZE));

	int commandId = 0;

	for (auto &[pos, meshAllocation] : chunkMeshesAllocations)
	{

		if (!culler.isVisible(pos))
			continue;

		indirectCommands[commandId++] = vk::DrawIndexedIndirectCommand{
				.indexCount = meshAllocation.indexCount,
				.instanceCount = 1,
				.firstIndex = static_cast<uint32_t>(
						meshAllocation.indexOffset / sizeof(uint32_t)),
				.vertexOffset = static_cast<int32_t>(
						meshAllocation.vertexOffset / sizeof(Vertex)),
				.firstInstance = 0,
		};

		totalIndices += meshAllocation.indexCount;
	}

	indirectBufferMemory.unmapMemory();

	commandBuffer.drawIndexedIndirect(*indirectBuffer, 0, commandId, sizeof(vk::DrawIndexedIndirectCommand));

	// GUI

	gui.beginFrame();

	// drawGUI();

	gui.endFrame(commandBuffer);

	// end GUI

	commandBuffer.endRendering();

	transition_image_layout(
			commandBuffer,
			vkSwapchain.getImages()[imageIndex],
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			{},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eBottomOfPipe,
			vk::ImageAspectFlagBits::eColor);

	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, frameIndex * 2 + 1);

	commandBuffer.end();

	// vkContext.getDevice().waitIdle();

	mainPipeline.getVkUbo().updateUniformBuffer(camera);

	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	const vk::SubmitInfo submitInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*vkSynchronization[frameIndex].getPresentCompleteSemaphore(),
			.pWaitDstStageMask = &waitDestinationStageMask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*vkSwapchain.getRenderFinishedSemaphore(imageIndex),
	};

	vkContext.getQueue().submit(submitInfo, *vkSynchronization[frameIndex].getDrawFence());

	const vk::PresentInfoKHR presentInfoKHR{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*vkSwapchain.getRenderFinishedSemaphore(imageIndex),
			.swapchainCount = 1,
			.pSwapchains = &*vkSwapchain.getSwapchain(),
			.pImageIndices = &imageIndex,
	};

	auto result = vkContext.getQueue().presentKHR(presentInfoKHR);

	if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR || window.framebufferResized)
	{
		window.framebufferResized = false;

		vkSwapchain.recreateSwapchain();
	}
	else
	{
		assert(result == vk::Result::eSuccess);
	}

	first[frameIndex] = false;
	frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

	auto end = std::chrono::high_resolution_clock::now();

	cpuMs = std::chrono::duration<double, std::milli>(end - start).count();
}

void Renderer::waitIdle()
{
	vkContext.getDevice().waitIdle();
}

void Renderer::drawGUI()
{
	ImGui::Text("CPU frame: %.2fms", cpuMs);
	ImGui::Text("GPU frame: %.2fms", gpuMs);
	ImGui::Text("Chunks loaded: %d", chunkMeshesAllocations.size());
	ImGui::Text("Load queue size: %d", loadQueueSize);
	ImGui::Text("Unload queue size: %d", unloadQueueSize);
	ImGui::Text("Triangles: %d", totalIndices / 3);
}
