#include <renderer/renderer.hpp>

#include <ecs/components/mesh_component.hpp>
#include <ecs/components/transform_component.hpp>
#include <renderer/push.hpp>
#include <cassert>
#include <core/chunk.hpp>
#include <core/block.hpp>
#include <iostream>
#include <algorithm>
#include <core/frustum_culler.hpp>
#include <renderer/mesh_allocator.hpp>

static_assert(BLOCK_TEXTURE_COUNT <= MAX_TEXTURES);

constexpr uint32_t MAX_DRAW_COUNT = 100000;
constexpr uint32_t MAX_CHUNK_SLOTS = MAX_DRAW_COUNT;
constexpr uint32_t NO_SLOT = UINT32_MAX;
constexpr vk::DeviceSize MAX_DRAW_SIZE = sizeof(vk::DrawIndexedIndirectCommand) * MAX_DRAW_COUNT;
constexpr vk::DeviceSize MAX_INSTANCE_SIZE = sizeof(ChunkInstance) * MAX_DRAW_COUNT;
constexpr vk::DeviceSize CHUNK_TABLE_SIZE = sizeof(ChunkCullData) * MAX_CHUNK_SLOTS;
constexpr vk::DeviceSize DRAW_COUNT_SIZE = sizeof(uint32_t) * MAX_FRAMES_IN_FLIGHT;

static_assert(sizeof(vk::DrawIndexedIndirectCommand) == 20);

Renderer::Renderer(Window &win, Scene &_scene, Camera &cam, ThreadQueue<ChunkMeshMessage> &mQueue, ThreadQueue<MeshAllocation> &fQueue)
		: window(win),
			vkContext(win),
			vkCommand(vkContext),
			vkResource(vkContext, vkCommand),
			vkSwapchain(vkContext, win, vkResource, vkSynchronization),
			scene(_scene),
			commandBuffers(vkCommand.createCommandBuffers(MAX_FRAMES_IN_FLIGHT)),
			mainPipeline(vkContext, vkSwapchain, vkResource),
			cullPipeline(vkContext, vkResource),
			camera(cam),
			queryPool(
					vkContext.getDevice(),
					vk::QueryPoolCreateInfo{}
							.setQueryType(vk::QueryType::eTimestamp)
							.setQueryCount(MAX_FRAMES_IN_FLIGHT * 2)),
			meshQueue(mQueue),
			freeQueue(fQueue),
			gui(window, vkContext, vkResource, vkSwapchain),
			vertexBuffer(vkResource.createBuffer(VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, true)),
			vertexBufferMemory(vkResource.getBufferMemory(vertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			indexBuffer(vkResource.createBuffer(INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, true)),
			indexBufferMemory(vkResource.getBufferMemory(indexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			indirectBuffer(vkResource.createBuffer(MAX_DRAW_SIZE * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer)),
			indirectBufferMemory(vkResource.getBufferMemory(indirectBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			instanceBuffer(vkResource.createBuffer(MAX_INSTANCE_SIZE * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer)),
			instanceBufferMemory(vkResource.getBufferMemory(instanceBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			drawCountBuffer(vkResource.createBuffer(DRAW_COUNT_SIZE, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)),
			drawCountBufferMemory(vkResource.getBufferMemory(drawCountBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			chunkTableBuffer(vkResource.createBuffer(CHUNK_TABLE_SIZE, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)),
			chunkTableBufferMemory(vkResource.getBufferMemory(chunkTableBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			chunkStagingBuffer(vkResource.createBuffer(CHUNK_TABLE_SIZE * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eTransferSrc)),
			chunkStagingBufferMemory(vkResource.getBufferMemory(chunkStagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)),
			textureManager(vkResource),
			sampler(createSampler())
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkSynchronization.emplace_back(vkContext);
	}

	chunkStaging = static_cast<ChunkCullData *>(chunkStagingBufferMemory.mapMemory(0, CHUNK_TABLE_SIZE * MAX_FRAMES_IN_FLIGHT));
	cullPipeline.bindBuffers(*chunkTableBuffer, *indirectBuffer, *instanceBuffer, *drawCountBuffer);

	for (uint32_t slot = 0; slot < BLOCK_TEXTURE_COUNT; slot++)
	{
		uint32_t id = loadTexture(BLOCK_TEXTURE_PATHS[slot]);
		assert(id == slot);
	}
}

uint32_t Renderer::loadTexture(const std::string &filename)
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

	return texture.id;
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

void Renderer::memoryBarrier(
		vk::raii::CommandBuffer &commandBuffer,
		vk::PipelineStageFlags2 src_stage_mask,
		vk::AccessFlags2 src_access_mask,
		vk::PipelineStageFlags2 dst_stage_mask,
		vk::AccessFlags2 dst_access_mask)
{
	vk::MemoryBarrier2 barrier = {
			.srcStageMask = src_stage_mask,
			.srcAccessMask = src_access_mask,
			.dstStageMask = dst_stage_mask,
			.dstAccessMask = dst_access_mask,
	};

	vk::DependencyInfo dependency_info = {
			.dependencyFlags = {},
			.memoryBarrierCount = 1,
			.pMemoryBarriers = &barrier,
	};

	commandBuffer.pipelineBarrier2(dependency_info);
}

void Renderer::applyMeshMessages()
{
	meshQueueSize = meshQueue.getSize();

	ChunkMeshMessage message;

	while (meshQueue.tryPop(message))
	{
		glm::ivec3 pos = std::holds_alternative<ChunkMeshLoad>(message)
												 ? std::get<ChunkMeshLoad>(message).allocation.pos
												 : std::get<ChunkMeshUnload>(message).pos;

		auto it = chunkMeshesAllocations.find(pos);

		if (it != chunkMeshesAllocations.end())
		{
			uint32_t slot = it->second.slot;

			if (slot != NO_SLOT)
			{
				dirtySlots[slot] = ChunkCullData{};
				freeSlots.push_back(slot);
			}

			totalIndices -= it->second.allocation.indexCount;
			unsubmittedFrees.push_back(it->second.allocation);
			chunkMeshesAllocations.erase(it);
		}

		auto *load = std::get_if<ChunkMeshLoad>(&message);

		if (!load)
			continue;

		const MeshAllocation &allocation = load->allocation;
		uint32_t slot = NO_SLOT;

		if (!freeSlots.empty())
		{
			slot = freeSlots.back();
			freeSlots.pop_back();
		}
		else if (slotCount < MAX_CHUNK_SLOTS)
		{
			slot = slotCount++;
		}

		if (slot != NO_SLOT)
		{
			assert(allocation.vertexOffset % sizeof(ChunkVertex) == 0);
			assert(allocation.indexOffset % sizeof(uint32_t) == 0);

			dirtySlots[slot] = ChunkCullData{
					.originScale = glm::vec4(glm::vec3(pos * CHUNK_SIZE * VOXEL_SIZE), static_cast<float>(VOXEL_SIZE)),
					.indexCount = allocation.indexCount,
					.firstIndex = static_cast<uint32_t>(allocation.indexOffset / sizeof(uint32_t)),
					.vertexOffset = static_cast<int32_t>(allocation.vertexOffset / sizeof(ChunkVertex)),
					.pad = 0,
			};
		}

		totalIndices += allocation.indexCount;
		chunkMeshesAllocations.emplace(pos, ChunkDrawEntry{.allocation = allocation, .slot = slot});
	}
}

void Renderer::recordCulling(vk::raii::CommandBuffer &commandBuffer)
{
	memoryBarrier(
			commandBuffer,
			vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eShaderStorageWrite | vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite);

	if (!dirtySlots.empty())
	{
		uint32_t stagingBase = MAX_CHUNK_SLOTS * frameIndex;
		std::vector<vk::BufferCopy> regions;
		regions.reserve(dirtySlots.size());

		for (auto &[slot, data] : dirtySlots)
		{
			uint32_t stagingIndex = stagingBase + static_cast<uint32_t>(regions.size());
			chunkStaging[stagingIndex] = data;

			regions.push_back(vk::BufferCopy{
					.srcOffset = sizeof(ChunkCullData) * stagingIndex,
					.dstOffset = sizeof(ChunkCullData) * slot,
					.size = sizeof(ChunkCullData),
			});
		}

		commandBuffer.copyBuffer(*chunkStagingBuffer, *chunkTableBuffer, regions);
		dirtySlots.clear();
	}

	commandBuffer.fillBuffer(*drawCountBuffer, sizeof(uint32_t) * frameIndex, sizeof(uint32_t), 0);

	memoryBarrier(
			commandBuffer,
			vk::PipelineStageFlagBits2::eTransfer,
			vk::AccessFlagBits2::eTransferWrite,
			vk::PipelineStageFlagBits2::eComputeShader,
			vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite);

	float aspectRatio = static_cast<float>(vkSwapchain.getExtent().width) / static_cast<float>(vkSwapchain.getExtent().height);
	FrustumCuller culler(camera, aspectRatio);

	CullPushConstants pushConstants{
			.slotCount = slotCount,
			.outputBase = MAX_DRAW_COUNT * frameIndex,
			.countIndex = frameIndex,
			.maxDraws = MAX_DRAW_COUNT,
			.chunkExtent = static_cast<float>(CHUNK_SIZE * VOXEL_SIZE),
	};

	const auto &planes = culler.getPlanes();

	for (uint32_t i = 0; i < 6; i++)
		pushConstants.planes[i] = glm::vec4(planes[i].normal, planes[i].d);

	cullPipeline.dispatch(commandBuffer, pushConstants);

	memoryBarrier(
			commandBuffer,
			vk::PipelineStageFlagBits2::eComputeShader,
			vk::AccessFlagBits2::eShaderStorageWrite,
			vk::PipelineStageFlagBits2::eDrawIndirect | vk::PipelineStageFlagBits2::eVertexAttributeInput,
			vk::AccessFlagBits2::eIndirectCommandRead | vk::AccessFlagBits2::eVertexAttributeRead);
}

void Renderer::render()
{
	static bool first[MAX_FRAMES_IN_FLIGHT] = {true, true};
	auto start = std::chrono::high_resolution_clock::now();

	vkSynchronization[frameIndex].waitDrawFence();

	for (auto &allocation : pendingFrees[frameIndex])
		freeQueue.push(std::move(allocation));

	pendingFrees[frameIndex].clear();

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

	applyMeshMessages();
	recordCulling(commandBuffer);

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
	vk::ClearValue clearDepth = vk::ClearDepthStencilValue(0.0f, 0);

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

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline.getEntityPipeline());
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mainPipeline.getPipelineLayout(), 0, *mainPipeline.getDescriptorSet(), mainPipeline.getVkUbo().getDynamicOffset(frameIndex));

	for (auto &entity : entities)
	{
		if (!entity->isActive())
			continue;

		auto meshComponent = entity->getComponent<MeshComponent>();
		auto transformComponent = entity->getComponent<TransformComponent>();

		if (!meshComponent || !transformComponent)
			continue;

		auto &mesh = meshComponent->getMesh();

		if (mesh.isEmpty())
			continue;

		auto &indices = mesh.getIndices();

		vk::DeviceSize vertexOffset = 0;
		commandBuffer.bindVertexBuffers(0, *mesh.getVertexBuffer(), vertexOffset);
		commandBuffer.bindIndexBuffer(*mesh.getIndexBuffer(), 0, vk::IndexTypeValue<uint32_t>::value);
		PushConstants pushConstants;

		pushConstants.model = transformComponent->getTransformMatrix();

		commandBuffer.pushConstants<PushConstants>(mainPipeline.getPipelineLayout(), vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
		commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
	}

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline.getPipeline());
	std::array<vk::Buffer, 2> chunkVertexBuffers{*vertexBuffer, *instanceBuffer};
	std::array<vk::DeviceSize, 2> chunkVertexOffsets{0, 0};
	commandBuffer.bindVertexBuffers(0, chunkVertexBuffers, chunkVertexOffsets);
	commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexTypeValue<uint32_t>::value);
	commandBuffer.drawIndexedIndirectCount(
			*indirectBuffer,
			MAX_DRAW_SIZE * frameIndex,
			*drawCountBuffer,
			sizeof(uint32_t) * frameIndex,
			MAX_DRAW_COUNT,
			sizeof(vk::DrawIndexedIndirectCommand));

	// GUI

	gui.beginFrame();

	// drawGUI();

	{
		std::lock_guard lock(vkContext.getQueueMutex());
		gui.endFrame(commandBuffer);
	}

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

	mainPipeline.getVkUbo().updateUniformBuffer(camera, frameIndex);

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

	const vk::PresentInfoKHR presentInfoKHR{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*vkSwapchain.getRenderFinishedSemaphore(imageIndex),
			.swapchainCount = 1,
			.pSwapchains = &*vkSwapchain.getSwapchain(),
			.pImageIndices = &imageIndex,
	};

	vk::Result result;
	{
		vkSynchronization[frameIndex].resetDrawFence();
		std::lock_guard lock(vkContext.getQueueMutex());
		vkContext.getQueue().submit(submitInfo, *vkSynchronization[frameIndex].getDrawFence());

		auto &frees = pendingFrees[frameIndex];
		frees.insert(frees.end(), std::make_move_iterator(unsubmittedFrees.begin()), std::make_move_iterator(unsubmittedFrees.end()));
		unsubmittedFrees.clear();

		result = vkContext.getQueue().presentKHR(presentInfoKHR);
	}

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
	vkContext.waitIdle();
}

void Renderer::drawGUI()
{
	ImGui::Text("CPU frame: %.2fms", cpuMs);
	ImGui::Text("GPU frame: %.2fms", gpuMs);
	ImGui::Text("Chunks loaded: %d", chunkMeshesAllocations.size());
	ImGui::Text("Mesh queue size: %d", meshQueueSize);
	ImGui::Text("Triangles: %d", totalIndices / 3);
}
