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
			gpuContext(win),
			gpuCommand(gpuContext),
			gpuResource(gpuContext, gpuCommand),
			gpuSwapchain(gpuContext, win, gpuResource, gpuSynchronization),
			scene(_scene),
			commandBuffers(gpuCommand.createCommandBuffers(MAX_FRAMES_IN_FLIGHT)),
			mainPipeline(gpuContext, gpuSwapchain, gpuResource),
			cullPipeline(gpuContext, gpuResource),
			camera(cam),
			queryPool(
					gpuContext.getDevice(),
					vk::QueryPoolCreateInfo{}
							.setQueryType(vk::QueryType::eTimestamp)
							.setQueryCount(MAX_FRAMES_IN_FLIGHT * 2)),
			meshQueue(mQueue),
			freeQueue(fQueue),
			gui(window, gpuContext, gpuResource, gpuSwapchain),
			vertexBuffer(gpuResource.createBuffer(VERTEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, true)),
			vertexBufferMemory(gpuResource.getBufferMemory(vertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			indexBuffer(gpuResource.createBuffer(INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, true)),
			indexBufferMemory(gpuResource.getBufferMemory(indexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			indirectBuffer(gpuResource.createBuffer(MAX_DRAW_SIZE * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer)),
			indirectBufferMemory(gpuResource.getBufferMemory(indirectBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			instanceBuffer(gpuResource.createBuffer(MAX_INSTANCE_SIZE * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eStorageBuffer)),
			instanceBufferMemory(gpuResource.getBufferMemory(instanceBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			drawCountBuffer(gpuResource.createBuffer(DRAW_COUNT_SIZE, vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)),
			drawCountBufferMemory(gpuResource.getBufferMemory(drawCountBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			chunkTableBuffer(gpuResource.createBuffer(CHUNK_TABLE_SIZE, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)),
			chunkTableBufferMemory(gpuResource.getBufferMemory(chunkTableBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal)),
			chunkStagingBuffer(gpuResource.createBuffer(CHUNK_TABLE_SIZE * MAX_FRAMES_IN_FLIGHT, vk::BufferUsageFlagBits::eTransferSrc)),
			chunkStagingBufferMemory(gpuResource.getBufferMemory(chunkStagingBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)),
			textureManager(gpuResource),
			sampler(createSampler())
{
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		gpuSynchronization.emplace_back(gpuContext);
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

	gpuContext.getDevice().updateDescriptorSets(write, {});

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
			gpuContext.getDevice(),
			samplerInfo,
	};
}

void Renderer::transitionImageLayout(
		vk::raii::CommandBuffer &commandBuffer,
		vk::Image image,
		vk::ImageLayout oldLayout,
		vk::ImageLayout newLayout,
		vk::AccessFlags2 srcAccessMask,
		vk::AccessFlags2 dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask,
		vk::PipelineStageFlags2 dstStageMask,
		vk::ImageAspectFlags imageAspectFlags)
{
	vk::ImageMemoryBarrier2 barrier = {
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {
					.aspectMask = imageAspectFlags,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
			},
	};

	vk::DependencyInfo dependencyInfo = {
			.dependencyFlags = {},
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier,
	};

	commandBuffer.pipelineBarrier2(dependencyInfo);
}

void Renderer::memoryBarrier(
		vk::raii::CommandBuffer &commandBuffer,
		vk::PipelineStageFlags2 srcStageMask,
		vk::AccessFlags2 srcAccessMask,
		vk::PipelineStageFlags2 dstStageMask,
		vk::AccessFlags2 dstAccessMask)
{
	vk::MemoryBarrier2 barrier = {
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
	};

	vk::DependencyInfo dependencyInfo = {
			.dependencyFlags = {},
			.memoryBarrierCount = 1,
			.pMemoryBarriers = &barrier,
	};

	commandBuffer.pipelineBarrier2(dependencyInfo);
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

	float aspectRatio = static_cast<float>(gpuSwapchain.getExtent().width) / static_cast<float>(gpuSwapchain.getExtent().height);
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

	gpuSynchronization[frameIndex].waitDrawFence();

	for (auto &allocation : pendingFrees[frameIndex])
		freeQueue.push(std::move(allocation));

	pendingFrees[frameIndex].clear();

	std::array<uint64_t, 2> timestamps{};

	if (!first[frameIndex])
	{
		auto queryPoolResult = (*gpuContext.getDevice()).getQueryPoolResults(*queryPool, frameIndex * 2, 2, sizeof(timestamps), timestamps.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64);

		gpuMs =
				(timestamps[1] - timestamps[0]) *
				gpuContext.getPhysicalDevice().getProperties().limits.timestampPeriod /
				1'000'000.0;
	}

	uint32_t imageIndex = gpuSwapchain.acquireNextImage(frameIndex);
	auto &commandBuffer = commandBuffers[frameIndex];

	commandBuffer.reset();

	auto &entities = scene.getEntities();

	commandBuffer.begin({});

	commandBuffer.resetQueryPool(*queryPool, frameIndex * 2, 2);
	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, *queryPool, frameIndex * 2);

	applyMeshMessages();
	recordCulling(commandBuffer);

	transitionImageLayout(
			commandBuffer,
			gpuSwapchain.getImages()[imageIndex],
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor);

	transitionImageLayout(
			commandBuffer,
			*gpuSwapchain.getColorImage(),
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor);

	transitionImageLayout(
			commandBuffer,
			*gpuSwapchain.getDepthImage(),
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
			.imageView = gpuSwapchain.getColorImageView(),
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.resolveMode = vk::ResolveModeFlagBits::eAverage,
			.resolveImageView = gpuSwapchain.getImageViews()[imageIndex],
			.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = clearColor,
	};

	vk::RenderingAttachmentInfo depthAttachmentInfo = {
			.imageView = gpuSwapchain.getDepthImageView(),
			.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eDontCare,
			.clearValue = clearDepth,
	};

	vk::RenderingInfo renderingInfo = {
			.renderArea = {.offset = {0, 0}, .extent = gpuSwapchain.getExtent()},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachmentInfo,
			.pDepthAttachment = &depthAttachmentInfo,
	};

	commandBuffer.beginRendering(renderingInfo);

	vk::Viewport viewport{
			0.0f,
			0.0f,
			static_cast<float>(gpuSwapchain.getExtent().width),
			static_cast<float>(gpuSwapchain.getExtent().height),
			0.0f,
			1.0f,
	};
	vk::Rect2D scissor{vk::Offset2D{0, 0}, gpuSwapchain.getExtent()};

	commandBuffer.setViewport(0, viewport);
	commandBuffer.setScissor(0, scissor);
	commandBuffer.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline.getEntityPipeline());
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mainPipeline.getPipelineLayout(), 0, *mainPipeline.getDescriptorSet(), mainPipeline.getGpuUbo().getDynamicOffset(frameIndex));

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

	drawGUI();

	{
		std::lock_guard lock(gpuContext.getQueueMutex());
		gui.endFrame(commandBuffer);
	}

	// end GUI

	commandBuffer.endRendering();

	transitionImageLayout(
			commandBuffer,
			gpuSwapchain.getImages()[imageIndex],
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			{},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eBottomOfPipe,
			vk::ImageAspectFlagBits::eColor);

	commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, frameIndex * 2 + 1);

	commandBuffer.end();


	mainPipeline.getGpuUbo().updateUniformBuffer(camera, frameIndex);

	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	const vk::SubmitInfo submitInfo{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*gpuSynchronization[frameIndex].getPresentCompleteSemaphore(),
			.pWaitDstStageMask = &waitDestinationStageMask,
			.commandBufferCount = 1,
			.pCommandBuffers = &*commandBuffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &*gpuSwapchain.getRenderFinishedSemaphore(imageIndex),
	};

	const vk::PresentInfoKHR presentInfoKHR{
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &*gpuSwapchain.getRenderFinishedSemaphore(imageIndex),
			.swapchainCount = 1,
			.pSwapchains = &*gpuSwapchain.getSwapchain(),
			.pImageIndices = &imageIndex,
	};

	vk::Result result;
	{
		gpuSynchronization[frameIndex].resetDrawFence();
		std::lock_guard lock(gpuContext.getQueueMutex());
		gpuContext.getQueue().submit(submitInfo, *gpuSynchronization[frameIndex].getDrawFence());

		auto &frees = pendingFrees[frameIndex];
		frees.insert(frees.end(), std::make_move_iterator(unsubmittedFrees.begin()), std::make_move_iterator(unsubmittedFrees.end()));
		unsubmittedFrees.clear();

		result = gpuContext.getQueue().presentKHR(presentInfoKHR);
	}

	if (result == vk::Result::eSuboptimalKHR || result == vk::Result::eErrorOutOfDateKHR || window.framebufferResized)
	{
		window.framebufferResized = false;

		gpuSwapchain.recreateSwapchain();
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
	gpuContext.waitIdle();
}

void Renderer::drawGUI()
{
	ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
	ImGui::Text("CPU frame: %.2fms", cpuMs);
	ImGui::Text("GPU frame: %.2fms", gpuMs);
	ImGui::Text("Chunks loaded: %zu", chunkMeshesAllocations.size());
	ImGui::Text("Mesh queue size: %u", meshQueueSize);
	ImGui::Text("Triangles: %u", totalIndices / 3);
	ImGui::End();
}
