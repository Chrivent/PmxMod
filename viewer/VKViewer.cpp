#include "VKViewer.h"

#include "../src/MMDReader.h"
#include "../src/MMDUtil.h"
#include "../src/MMDModel.h"
#include "../src/VMDAnimation.h"

#include "../external/stb_image.h"

#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <ranges>

bool FindMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& memProps,
                         const uint32_t typeBits, const vk::MemoryPropertyFlags reqMask, uint32_t* typeIndex) {
	if (typeIndex == nullptr)
		return false;
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
		if ((typeBits >> i & 1) == 1) {
			if ((memProps.memoryTypes[i].propertyFlags & reqMask) == reqMask) {
				*typeIndex = i;
				return true;
			}
		}
	}
	return false;
}

void SetImageLayout(const vk::CommandBuffer cmdBuf, const vk::Image image,
                    const vk::ImageLayout oldImageLayout, const vk::ImageLayout newImageLayout,
                    const vk::ImageSubresourceRange& subresourceRange) {
	auto imageMemoryBarrier = vk::ImageMemoryBarrier()
			.setOldLayout(oldImageLayout)
			.setNewLayout(newImageLayout)
			.setImage(image)
			.setSubresourceRange(subresourceRange);
	switch (oldImageLayout) {
		case vk::ImageLayout::eUndefined:
			imageMemoryBarrier.setSrcAccessMask(vk::AccessFlags());
			break;
		case vk::ImageLayout::eTransferDstOptimal:
			imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			break;
		case vk::ImageLayout::ePreinitialized:
			imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eHostWrite);
			break;
		default:
			break;
	}
	switch (newImageLayout) {
		case vk::ImageLayout::eShaderReadOnlyOptimal:
			imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			break;
		case vk::ImageLayout::eTransferSrcOptimal:
			imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);
			break;
		case vk::ImageLayout::eTransferDstOptimal:
			imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			break;
		default:
			break;
	}
	vk::PipelineStageFlags srcStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
	vk::PipelineStageFlags destStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
	if (oldImageLayout == vk::ImageLayout::eUndefined &&
	    newImageLayout == vk::ImageLayout::eTransferDstOptimal) {
		srcStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
		destStageFlags = vk::PipelineStageFlagBits::eTransfer;
	} else if (oldImageLayout == vk::ImageLayout::eTransferDstOptimal &&
	           newImageLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		srcStageFlags = vk::PipelineStageFlagBits::eTransfer;
		destStageFlags = vk::PipelineStageFlagBits::eFragmentShader;
	}
	cmdBuf.pipelineBarrier(
		srcStageFlags,
		destStageFlags,
		vk::DependencyFlags(),
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier
	);
}

void VKSwapChainImageResource::Clear(const VKAppContext& appContext) {
	const auto device = appContext.m_device;
	const auto commandPool = appContext.m_commandPool;
	device.destroyImageView(m_imageView, nullptr);
	m_imageView = nullptr;
	device.destroyFramebuffer(m_framebuffer, nullptr);
	m_framebuffer = nullptr;
	device.freeCommandBuffers(commandPool, 1, &m_cmdBuffer);
	m_cmdBuffer = nullptr;
}

bool VKBuffer::Setup(
	const VKAppContext&				appContext,
	const vk::DeviceSize			memSize,
	const vk::BufferUsageFlags	usage
) {
	const auto device = appContext.m_device;
	Clear(appContext);
	const auto bufferInfo = vk::BufferCreateInfo()
			.setSize(memSize)
			.setUsage(usage);
	vk::Result ret = device.createBuffer(&bufferInfo, nullptr, &m_buffer);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Buffer.\n";
		return false;
	}
	const auto bufMemReq = device.getBufferMemoryRequirements(m_buffer);
	uint32_t bufMemTypeIndex;
	if (!FindMemoryTypeIndex(
			appContext.m_memProperties,
			bufMemReq.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			&bufMemTypeIndex)
	) {
		std::cout << "Failed to found Memory Type Index.\n";
		return false;
	}
	const auto bufMemAllocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(bufMemReq.size)
			.setMemoryTypeIndex(bufMemTypeIndex);
	ret = device.allocateMemory(&bufMemAllocInfo, nullptr, &m_memory);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to allocate Memory.\n";
		return false;
	}
	device.bindBufferMemory(m_buffer, m_memory, 0);
	m_memorySize = memSize;
	return true;
}

void VKBuffer::Clear(const VKAppContext& appContext) {
	const auto device = appContext.m_device;
	if (m_buffer) {
		device.destroyBuffer(m_buffer, nullptr);
		m_buffer = nullptr;
	}
	if (m_memory) {
		device.freeMemory(m_memory, nullptr);
		m_memory = nullptr;
	}
	m_memorySize = 0;
}

bool VKTexture::Setup(const VKAppContext& appContext, const uint32_t width, const uint32_t height,
                    const vk::Format format, const int mipCount) {
	const auto device = appContext.m_device;
	const auto imageInfo = vk::ImageCreateInfo()
			.setImageType(vk::ImageType::e2D)
			.setFormat(format)
			.setMipLevels(mipCount)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setTiling(vk::ImageTiling::eOptimal)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setExtent(vk::Extent3D(width, height, 1))
			.setArrayLayers(1)
			.setUsage(vk::ImageUsageFlagBits::eTransferDst |
				vk::ImageUsageFlagBits::eTransferSrc |
				vk::ImageUsageFlagBits::eSampled);
	vk::Result ret = device.createImage(&imageInfo, nullptr, &m_image);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Image.\n";
		return false;
	}
	const auto memReq = device.getImageMemoryRequirements(m_image);
	uint32_t memTypeIndex;
	if (!FindMemoryTypeIndex(
		appContext.m_memProperties,
		memReq.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&memTypeIndex)) {
		std::cout << "Failed to find Memory Type Index.\n";
		return false;
	}
	const auto memAllocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(memReq.size)
			.setMemoryTypeIndex(memTypeIndex);
	ret = device.allocateMemory(&memAllocInfo, nullptr, &m_memory);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to allocate memory.\n";
		return false;
	}
	device.bindImageMemory(m_image, m_memory, 0);
	const auto imageViewInfo = vk::ImageViewCreateInfo()
			.setFormat(format)
			.setViewType(vk::ImageViewType::e2D)
			.setComponents(vk::ComponentMapping(
				vk::ComponentSwizzle::eR,
				vk::ComponentSwizzle::eG,
				vk::ComponentSwizzle::eB,
				vk::ComponentSwizzle::eA))
			.setSubresourceRange(vk::ImageSubresourceRange()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setBaseMipLevel(0)
				.setBaseArrayLayer(0)
				.setLayerCount(1)
				.setLevelCount(mipCount))
			.setImage(m_image);
	ret = device.createImageView(&imageViewInfo, nullptr, &m_imageView);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Image View.\n";
		return false;
	}
	m_format = format;
	m_mipLevel = mipCount;
	return true;
}

void VKTexture::Clear(const VKAppContext& appContext) {
	const auto device = appContext.m_device;
	device.destroyImage(m_image, nullptr);
	m_image = nullptr;
	device.destroyImageView(m_imageView, nullptr);
	m_imageView = nullptr;
	m_imageLayout = vk::ImageLayout::eUndefined;
	device.freeMemory(m_memory, nullptr);
	m_memory = nullptr;
}

bool VKStagingBuffer::Setup(const VKAppContext& appContext, const vk::DeviceSize size) {
	const auto device = appContext.m_device;
	if (size <= m_memorySize)
		return true;
	Clear(appContext);
	const auto bufInfo = vk::BufferCreateInfo()
			.setSize(size)
			.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
	vk::Result ret = device.createBuffer(&bufInfo, nullptr, &m_buffer);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Staging Buffer.\n";
		return false;
	}
	const auto bufMemReq = device.getBufferMemoryRequirements(m_buffer);
	uint32_t bufMemTypeIndex;
	if (!FindMemoryTypeIndex(
			appContext.m_memProperties,
			bufMemReq.memoryTypeBits,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			&bufMemTypeIndex)
	) {
		std::cout << "Failed to found Staging Buffer Memory Type Index.\n";
		return false;
	}
	const auto memAllocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(bufMemReq.size)
			.setMemoryTypeIndex(bufMemTypeIndex);
	ret = device.allocateMemory(&memAllocInfo, nullptr, &m_memory);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to allocate Staging Buffer Memory.\n";
		return false;
	}
	device.bindBufferMemory(m_buffer, m_memory, 0);
	m_memorySize = static_cast<uint32_t>(bufMemReq.size);
	const auto cmdPool = appContext.m_transferCommandPool;
	const auto cmdBufAllocInfo = vk::CommandBufferAllocateInfo()
			.setCommandPool(cmdPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
	ret = device.allocateCommandBuffers(&cmdBufAllocInfo, &m_copyCommand);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to allocate Staging Buffer Copy Command Buffer.\n";
		return false;
	}
	constexpr auto fenceInfo = vk::FenceCreateInfo()
			.setFlags(vk::FenceCreateFlagBits::eSignaled);
	ret = device.createFence(&fenceInfo, nullptr, &m_transferCompleteFence);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to  create Staging Buffer Transfer Complete Fence.\n";
		return false;
	}
	constexpr auto semaphoreInfo = vk::SemaphoreCreateInfo();
	ret = device.createSemaphore(&semaphoreInfo, nullptr, &m_transferCompleteSemaphore);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to  create Staging Buffer Transer Complete Semaphore.\n";
		return false;
	}
	return true;
}

void VKStagingBuffer::Clear(const VKAppContext& appContext) {
	const vk::Device device = appContext.m_device;
	Wait(appContext);
	device.destroyFence(m_transferCompleteFence, nullptr);
	m_transferCompleteFence = nullptr;
	device.destroySemaphore(m_transferCompleteSemaphore, nullptr);
	m_transferCompleteSemaphore = nullptr;
	m_waitSemaphore = nullptr;
	const auto cmdPool = appContext.m_transferCommandPool;
	device.freeCommandBuffers(cmdPool, 1, &m_copyCommand);
	m_copyCommand = nullptr;
	device.freeMemory(m_memory, nullptr);
	m_memory = nullptr;
	device.destroyBuffer(m_buffer, nullptr);
	m_buffer = nullptr;
}

void VKStagingBuffer::Wait(const VKAppContext& appContext) const {
	const auto device = appContext.m_device;
	if (m_transferCompleteFence) {
		const vk::Result res = device.waitForFences(1, &m_transferCompleteFence, true, -1);
		if (res != vk::Result::eSuccess)
			std::cout << "Failed to wait for Staging Buffer.\n";
	}
}

bool VKStagingBuffer::CopyBuffer(const VKAppContext& appContext, const vk::Buffer destBuffer, const vk::DeviceSize size) {
	constexpr auto cmdBufInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	vk::Result ret = m_copyCommand.begin(&cmdBufInfo);
	const auto copyRegion = vk::BufferCopy()
			.setSize(size);
	m_copyCommand.copyBuffer(m_buffer, destBuffer, 1, &copyRegion);
	m_copyCommand.end();
	auto submitInfo = vk::SubmitInfo()
			.setCommandBufferCount(1)
			.setPCommandBuffers(&m_copyCommand)
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&m_transferCompleteSemaphore);
	constexpr vk::PipelineStageFlags waitDstStage = vk::PipelineStageFlagBits::eTransfer;
	if (m_waitSemaphore) {
		submitInfo
				.setWaitSemaphoreCount(1)
				.setPWaitSemaphores(&m_waitSemaphore)
				.setPWaitDstStageMask(&waitDstStage);
	}
	const auto queue = appContext.m_graphicsQueue;
	ret = queue.submit(1, &submitInfo, m_transferCompleteFence);
	m_waitSemaphore = m_transferCompleteSemaphore;
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to submit Copy Command Buffer.\n";
		return false;
	}
	return true;
}

bool VKStagingBuffer::CopyImage(
	const VKAppContext& appContext,
	const vk::Image destImage,
	const vk::ImageLayout imageLayout,
	const uint32_t regionCount,
	const vk::BufferImageCopy* regions
) {
	constexpr auto cmdBufInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
	vk::Result ret = m_copyCommand.begin(&cmdBufInfo);
	const auto subresourceRange = vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setBaseMipLevel(0)
			.setLevelCount(regionCount)
			.setLayerCount(1);
	SetImageLayout(
		m_copyCommand,
		destImage,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eTransferDstOptimal,
		subresourceRange
	);
	m_copyCommand.copyBufferToImage(
		m_buffer,
		destImage,
		vk::ImageLayout::eTransferDstOptimal,
		regionCount,
		regions
	);
	SetImageLayout(
		m_copyCommand,
		destImage,
		vk::ImageLayout::eTransferDstOptimal,
		imageLayout,
		subresourceRange
	);
	m_copyCommand.end();
	auto submitInfo = vk::SubmitInfo()
			.setCommandBufferCount(1)
			.setPCommandBuffers(&m_copyCommand)
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&m_transferCompleteSemaphore);
	constexpr vk::PipelineStageFlags waitDstStage = vk::PipelineStageFlagBits::eTransfer;
	if (m_waitSemaphore)
		submitInfo.setWaitSemaphoreCount(1).setPWaitSemaphores(&m_waitSemaphore).setPWaitDstStageMask(&waitDstStage);
	const auto queue = appContext.m_graphicsQueue;
	ret = queue.submit(1, &submitInfo, m_transferCompleteFence);
	m_waitSemaphore = m_transferCompleteSemaphore;
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to submit Copy Command Buffer.\n";
		return false;
	}
	return true;
}

bool VKAppContext::Setup(const vk::Instance inst, const vk::SurfaceKHR surface, const vk::PhysicalDevice gpu, const vk::Device device) {
	m_vkInst = inst;
	m_surface = surface;
	m_gpu = gpu;
	m_device = device;
	m_resourceDir = PathUtil::GetExecutablePath();
	m_resourceDir = m_resourceDir.parent_path();
	m_resourceDir /= "resource";
	m_shaderDir = m_resourceDir / "shader_VK";
	m_mmdDir = m_resourceDir / "mmd";
	const auto memProperties = m_gpu.getMemoryProperties();
	m_memProperties = memProperties;
	const auto queueFamilies = gpu.getQueueFamilyProperties();
	if (queueFamilies.empty())
		return false;
	uint32_t selectGraphicsQueueFamilyIndex = UINT32_MAX;
	uint32_t selectPresentQueueFamilyIndex = UINT32_MAX;
	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); i++) {
		const auto& queue = queueFamilies[i];
		if (queue.queueFlags & vk::QueueFlagBits::eGraphics) {
			if (selectGraphicsQueueFamilyIndex == UINT32_MAX)
				selectGraphicsQueueFamilyIndex = i;
			if (gpu.getSurfaceSupportKHR(i, surface)) {
				selectGraphicsQueueFamilyIndex = i;
				selectPresentQueueFamilyIndex = i;
				break;
			}
		}
	}
	if (selectGraphicsQueueFamilyIndex == UINT32_MAX) {
		std::cout << "Failed to find graphics queue family.\n";
		return false;
	}
	if (selectPresentQueueFamilyIndex == UINT32_MAX) {
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); i++) {
			if (gpu.getSurfaceSupportKHR(i, surface)) {
				selectPresentQueueFamilyIndex = i;
				break;
			}
		}
	}
	if (selectPresentQueueFamilyIndex == UINT32_MAX) {
		std::cout << "Failed to find present queue family.\n";
		return false;
	}
	m_graphicsQueueFamilyIndex = selectGraphicsQueueFamilyIndex;
	m_presentQueueFamilyIndex = selectPresentQueueFamilyIndex;
	m_device.getQueue(m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);
	const auto surfaceCaps = m_gpu.getSurfaceCapabilitiesKHR(m_surface);
	m_imageCount = std::max(surfaceCaps.minImageCount, static_cast<uint32_t>(2));
	if (surfaceCaps.maxImageCount > DefaultImageCount)
		m_imageCount = DefaultImageCount;
	vk::Format selectColorFormats[] = {
		vk::Format::eB8G8R8A8Unorm,
		vk::Format::eB8G8R8A8Srgb,
	};
	m_colorFormat = vk::Format::eUndefined;
	const auto surfaceFormats = m_gpu.getSurfaceFormatsKHR(m_surface);
	for (const auto& selectFmt : selectColorFormats) {
		for (const auto& surfaceFmt : surfaceFormats) {
			if (selectFmt == surfaceFmt.format) {
				m_colorFormat = selectFmt;
				break;
			}
		}
		if (m_colorFormat != vk::Format::eUndefined)
			break;
	}
	if (m_colorFormat == vk::Format::eUndefined) {
		std::cout << "Failed to find color formant.\n";
		return false;
	}
	m_depthFormat = vk::Format::eUndefined;
	vk::Format selectDepthFormats[] = {
		vk::Format::eD24UnormS8Uint,
		vk::Format::eD16UnormS8Uint,
		vk::Format::eD32SfloatS8Uint,
	};
	for (const auto& selectFmt : selectDepthFormats) {
		auto fmtProp = m_gpu.getFormatProperties(selectFmt);
		if (fmtProp.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) {
			m_depthFormat = selectFmt;
			break;
		}
	}
	if (m_depthFormat == vk::Format::eUndefined) {
		std::cout << "Failed to find depth formant.\n";
		return false;
	}
	std::cout << "Select color format [" << static_cast<int>(m_colorFormat) << "]\n";
	std::cout << "Select depth format [" << static_cast<int>(m_depthFormat) << "]\n";
	if (!Prepare())
		return false;
	return true;
}

void VKAppContext::Destroy() {
	m_vmdCameraAnim.reset();
	if (!m_device)
		return;
	m_device.waitIdle();
	for (auto& [m_fence, m_presentCompleteSemaphore, m_renderCompleteSemaphore] : m_frameSyncDatas) {
		m_device.destroySemaphore(m_presentCompleteSemaphore, nullptr);
		m_presentCompleteSemaphore = nullptr;
		m_device.destroySemaphore(m_renderCompleteSemaphore, nullptr);
		m_renderCompleteSemaphore = nullptr;
		m_device.destroyFence(m_fence, nullptr);
		m_fence = nullptr;
	}
	if (m_swapChain)
		m_device.destroySwapchainKHR(m_swapChain, nullptr);
	for (auto& res : m_swapChainImageResources)
		res.Clear(*this);
	m_swapChainImageResources.clear();
	m_device.destroyImage(m_depthImage, nullptr);
	m_depthImage = nullptr;
	m_device.freeMemory(m_depthMem, nullptr);
	m_depthMem = nullptr;
	m_device.destroyImageView(m_depthImageView, nullptr);
	m_depthImageView = nullptr;
	m_device.destroyImageView(m_msaaColorImageView, nullptr);
	m_msaaColorImageView = nullptr;
	m_device.destroyImage(m_msaaColorImage, nullptr);
	m_msaaColorImage = nullptr;
	m_device.freeMemory(m_msaaColorMem, nullptr);
	m_msaaColorMem = nullptr;
	m_device.destroyImageView(m_msaaDepthImageView, nullptr);
	m_msaaDepthImageView = nullptr;
	m_device.destroyImage(m_msaaDepthImage, nullptr);
	m_msaaDepthImage = nullptr;
	m_device.freeMemory(m_msaaDepthMem, nullptr);
	m_msaaDepthMem = nullptr;
	m_device.destroyRenderPass(m_renderPass, nullptr);
	m_renderPass = nullptr;
	m_device.destroyDescriptorSetLayout(m_mmdDescriptorSetLayout, nullptr);
	m_mmdDescriptorSetLayout = nullptr;
	m_device.destroyPipelineLayout(m_mmdPipelineLayout, nullptr);
	m_mmdPipelineLayout = nullptr;
	for (auto& pipeline : m_mmdPipelines) {
		m_device.destroyPipeline(pipeline, nullptr);
		pipeline = nullptr;
	}
	m_device.destroyShaderModule(m_mmdVSModule, nullptr);
	m_mmdVSModule = nullptr;
	m_device.destroyShaderModule(m_mmdFSModule, nullptr);
	m_mmdFSModule = nullptr;
	m_device.destroyDescriptorSetLayout(m_mmdEdgeDescriptorSetLayout, nullptr);
	m_mmdEdgeDescriptorSetLayout = nullptr;
	m_device.destroyPipelineLayout(m_mmdEdgePipelineLayout, nullptr);
	m_mmdEdgePipelineLayout = nullptr;
	m_device.destroyPipeline(m_mmdEdgePipeline, nullptr);
	m_mmdEdgePipeline = nullptr;
	m_device.destroyShaderModule(m_mmdEdgeVSModule, nullptr);
	m_mmdEdgeVSModule = nullptr;
	m_device.destroyShaderModule(m_mmdEdgeFSModule, nullptr);
	m_mmdEdgeFSModule = nullptr;
	m_device.destroyDescriptorSetLayout(m_mmdGroundShadowDescriptorSetLayout, nullptr);
	m_mmdGroundShadowDescriptorSetLayout = nullptr;
	m_device.destroyPipelineLayout(m_mmdGroundShadowPipelineLayout, nullptr);
	m_mmdGroundShadowPipelineLayout = nullptr;
	m_device.destroyPipeline(m_mmdGroundShadowPipeline, nullptr);
	m_mmdGroundShadowPipeline = nullptr;
	m_device.destroyShaderModule(m_mmdGroundShadowVSModule, nullptr);
	m_mmdGroundShadowVSModule = nullptr;
	m_device.destroyShaderModule(m_mmdGroundShadowFSModule, nullptr);
	m_mmdGroundShadowFSModule = nullptr;
	m_device.destroyPipelineCache(m_pipelineCache, nullptr);
	m_pipelineCache = nullptr;
	m_graphicsQueue = nullptr;
	for (const auto& stBuf : m_stagingBuffers)
		stBuf->Clear(*this);
	m_stagingBuffers.clear();
	for (const auto& val : m_textures | std::views::values)
		val->Clear(*this);
	m_textures.clear();
	if (m_defaultTexture) {
		m_defaultTexture->Clear(*this);
		m_defaultTexture.reset();
	}
	m_device.destroySampler(m_defaultSampler, nullptr);
	m_defaultSampler = nullptr;
	m_device.destroyCommandPool(m_commandPool, nullptr);
	m_commandPool = nullptr;
	m_device.destroyCommandPool(m_transferCommandPool, nullptr);
	m_transferCommandPool = nullptr;
}

bool VKAppContext::Prepare() {
	if (!PrepareCommandPool()) return false;
	if (!PrepareBuffer()) return false;
	if (!PrepareSyncObjects()) return false;
	if (!PrepareRenderPass()) return false;
	if (!PrepareFramebuffer()) return false;
	if (!PreparePipelineCache()) return false;
	if (!PrepareMMDPipeline()) return false;
	if (!PrepareMMDEdgePipeline()) return false;
	if (!PrepareMMDGroundShadowPipeline()) return false;
	if (!PrepareDefaultTexture()) return false;
	return true;
}

bool VKAppContext::PrepareCommandPool() {
	const auto cmdPoolInfo = vk::CommandPoolCreateInfo()
			.setQueueFamilyIndex(m_graphicsQueueFamilyIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	vk::Result ret = m_device.createCommandPool(&cmdPoolInfo, nullptr, &m_commandPool);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Command Pool.\n";
		return false;
	}
	const auto transferCmdPoolInfo = vk::CommandPoolCreateInfo()
			.setQueueFamilyIndex(m_graphicsQueueFamilyIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	ret = m_device.createCommandPool(&transferCmdPoolInfo, nullptr, &m_transferCommandPool);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Transfer Command Pool.\n";
		return false;
	}
	return true;
}

bool VKAppContext::PrepareBuffer() {
	for (auto& res : m_swapChainImageResources)
		res.Clear(*this);
	m_swapChainImageResources.clear();
	m_device.destroyImageView(m_depthImageView, nullptr);
	m_depthImageView = nullptr;
	m_device.destroyImage(m_depthImage, nullptr);
	m_depthImage = nullptr;
	m_device.freeMemory(m_depthMem, nullptr);
	m_depthMem = nullptr;
	m_device.destroyImageView(m_msaaColorImageView, nullptr);
	m_msaaColorImageView = nullptr;
	m_device.destroyImage(m_msaaColorImage, nullptr);
	m_msaaColorImage = nullptr;
	m_device.freeMemory(m_msaaColorMem, nullptr);
	m_msaaColorMem = nullptr;
	m_device.destroyImageView(m_msaaDepthImageView, nullptr);
	m_msaaDepthImageView = nullptr;
	m_device.destroyImage(m_msaaDepthImage, nullptr);
	m_msaaDepthImage = nullptr;
	m_device.freeMemory(m_msaaDepthMem, nullptr);
	m_msaaDepthMem = nullptr;
	auto oldSwapChain = m_swapChain;
	auto surfaceCaps = m_gpu.getSurfaceCapabilitiesKHR(m_surface);
	vk::PresentModeKHR selectPresentModes[] = {
		vk::PresentModeKHR::eMailbox,
		vk::PresentModeKHR::eImmediate,
		vk::PresentModeKHR::eFifo,
	};
	auto presentModes = m_gpu.getSurfacePresentModesKHR(m_surface);
	bool findPresentMode = false;
	vk::PresentModeKHR selectPresentMode;
	for (auto selectMode : selectPresentModes) {
		for (auto presentMode : presentModes) {
			if (selectMode == presentMode) {
				selectPresentMode = selectMode;
				findPresentMode = true;
				break;
			}
		}
		if (findPresentMode)
			break;
	}
	if (!findPresentMode) {
		std::cout << "Present mode unsupported.\n";
		return false;
	}
	std::cout << "Select present mode [" << static_cast<int>(selectPresentMode) << "]\n";
	auto formats = m_gpu.getSurfaceFormatsKHR(m_surface);
	auto format = vk::Format::eB8G8R8A8Unorm;
	uint32_t selectFmtIdx = UINT32_MAX;
	for (uint32_t i = 0; i < static_cast<uint32_t>(formats.size()); i++) {
		if (formats[i].format == format) {
			selectFmtIdx = i;
			break;
		}
	}
	if (selectFmtIdx == UINT32_MAX) {
		std::cout << "Failed to find surface format.\n";
		return false;
	}
	auto colorSpace = formats[selectFmtIdx].colorSpace;
	auto compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	vk::CompositeAlphaFlagBitsKHR compositeAlphaFlags[] = {
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
		vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
		vk::CompositeAlphaFlagBitsKHR::eInherit
	};
	for (const auto& flag : compositeAlphaFlags) {
		if (surfaceCaps.supportedCompositeAlpha & flag) {
			compositeAlpha = flag;
			break;
		}
	}
	uint32_t indices[] = {
		m_graphicsQueueFamilyIndex,
		m_presentQueueFamilyIndex,
	};
	uint32_t* queueFamilyIndices = nullptr;
	uint32_t queueFamilyCount = 0;
	auto sharingMode = vk::SharingMode::eExclusive;
	if (m_graphicsQueueFamilyIndex != m_presentQueueFamilyIndex) {
		queueFamilyIndices = indices;
		queueFamilyCount = std::extent_v<decltype(indices)>;
		sharingMode = vk::SharingMode::eConcurrent;
	}
	auto swapChainInfo = vk::SwapchainCreateInfoKHR()
			.setSurface(m_surface)
			.setMinImageCount(m_imageCount)
			.setImageFormat(format)
			.setImageColorSpace(colorSpace)
			.setImageExtent(surfaceCaps.currentExtent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
			.setImageSharingMode(sharingMode)
			.setQueueFamilyIndexCount(queueFamilyCount)
			.setPQueueFamilyIndices(queueFamilyIndices)
			.setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
			.setCompositeAlpha(compositeAlpha)
			.setPresentMode(selectPresentMode)
			.setClipped(true)
			.setOldSwapchain(oldSwapChain);
	vk::SwapchainKHR swapChain;
	vk::Result ret;
	ret = m_device.createSwapchainKHR(&swapChainInfo, nullptr, &swapChain);
	if (ret != vk::Result::eSuccess) {
		std::cout << "Failed to create Swap Chain.\n";
		return false;
	}
	m_screenWidth = static_cast<int>(surfaceCaps.currentExtent.width);
	m_screenHeight = static_cast<int>(surfaceCaps.currentExtent.height);
	m_swapChain = swapChain;
	if (oldSwapChain)
		m_device.destroySwapchainKHR(oldSwapChain);
	auto swapChainImages = m_device.getSwapchainImagesKHR(swapChain);
	m_swapChainImageResources.resize(swapChainImages.size());
	for (size_t i = 0; i < swapChainImages.size(); i++) {
		m_swapChainImageResources[i].m_image = swapChainImages[i];
		auto imageViewInfo = vk::ImageViewCreateInfo()
				.setViewType(vk::ImageViewType::e2D)
				.setFormat(format)
				.setImage(swapChainImages[i])
				.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,
				                                               0, 1, 0, 1));
		vk::ImageView imageView;
		ret = m_device.createImageView(&imageViewInfo, nullptr, &imageView);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Image View.\n";
			return false;
		}
		m_swapChainImageResources[i].m_imageView = imageView;
		auto cmdBufInfo = vk::CommandBufferAllocateInfo()
				.setCommandBufferCount(1)
				.setCommandPool(m_commandPool)
				.setLevel(vk::CommandBufferLevel::ePrimary);
		vk::CommandBuffer cmdBuf;
		ret = m_device.allocateCommandBuffers(&cmdBufInfo, &cmdBuf);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Command Buffer.\n";
			return false;
		}
		m_swapChainImageResources[i].m_cmdBuffer = cmdBuf;
	}
	auto depthFormant = m_depthFormat;
	auto depthImageInfo = vk::ImageCreateInfo()
			.setImageType(vk::ImageType::e2D)
			.setFormat(depthFormant)
			.setExtent({ static_cast<uint32_t>(m_screenWidth), static_cast<uint32_t>(m_screenHeight), 1 })
			.setMipLevels(1)
			.setArrayLayers(1)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
			.setPQueueFamilyIndices(nullptr)
			.setQueueFamilyIndexCount(0)
			.setSharingMode(vk::SharingMode::eExclusive);
	vk::Image depthImage;
	ret = m_device.createImage(&depthImageInfo, nullptr, &depthImage);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Image.\n";
		return false;
	}
	m_depthImage = depthImage;
	auto depthMemoReq = m_device.getImageMemoryRequirements(depthImage);
	uint32_t depthMemIdx = 0;
	if (!FindMemoryTypeIndex(
		m_memProperties,
		depthMemoReq.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&depthMemIdx)) {
		std::cout << "Failed to find memory property.\n";
		return false;
	}
	auto depthMemAllocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(depthMemoReq.size)
			.setMemoryTypeIndex(depthMemIdx);
	vk::DeviceMemory depthMem;
	ret = m_device.allocateMemory(&depthMemAllocInfo, nullptr, &depthMem);
	if (ret != vk::Result::eSuccess) {
		std::cout << "Failed to allocate depth memory.\n";
		return false;
	}
	m_depthMem = depthMem;
	m_device.bindImageMemory(depthImage, depthMem, 0);
	auto depthImageViewInfo = vk::ImageViewCreateInfo()
			.setImage(depthImage)
			.setFormat(m_depthFormat)
			.setViewType(vk::ImageViewType::e2D)
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth,
			                                               0, 1, 0, 1));
	vk::ImageView depthImageView;
	ret = m_device.createImageView(&depthImageViewInfo, nullptr, &depthImageView);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Image View.\n";
		return false;
	}
	m_depthImageView = depthImageView;
	auto msaaColorFormant = m_colorFormat;
	auto msaaColorImageInfo = vk::ImageCreateInfo()
			.setImageType(vk::ImageType::e2D)
			.setFormat(msaaColorFormant)
			.setExtent({ static_cast<uint32_t>(m_screenWidth), static_cast<uint32_t>(m_screenHeight), 1 })
			.setMipLevels(1)
			.setArrayLayers(1)
			.setSamples(m_msaaSampleCount)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(vk::ImageUsageFlagBits::eColorAttachment)
			.setPQueueFamilyIndices(nullptr)
			.setQueueFamilyIndexCount(0)
			.setSharingMode(vk::SharingMode::eExclusive);
	vk::Image msaaColorImage;
	ret = m_device.createImage(&msaaColorImageInfo, nullptr, &msaaColorImage);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MSAA Color Image.\n";
		return false;
	}
	m_msaaColorImage = msaaColorImage;
	auto msaaColorMemoReq = m_device.getImageMemoryRequirements(msaaColorImage);
	uint32_t msaaColorMemIdx = 0;
	if (!FindMemoryTypeIndex(
		m_memProperties,
		msaaColorMemoReq.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&msaaColorMemIdx)) {
		std::cout << "Failed to find MSAA Color memory property.\n";
		return false;
	}
	auto msaaColorMemAllocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(msaaColorMemoReq.size)
			.setMemoryTypeIndex(msaaColorMemIdx);
	vk::DeviceMemory msaaColorMem;
	ret = m_device.allocateMemory(&msaaColorMemAllocInfo, nullptr, &msaaColorMem);
	if (ret != vk::Result::eSuccess) {
		std::cout << "Failed to allocate MSAA Color memory.\n";
		return false;
	}
	m_msaaColorMem = msaaColorMem;
	m_device.bindImageMemory(msaaColorImage, msaaColorMem, 0);
	auto msaaColorImageViewInfo = vk::ImageViewCreateInfo()
			.setImage(msaaColorImage)
			.setFormat(msaaColorFormant)
			.setViewType(vk::ImageViewType::e2D)
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor,
			                                               0, 1, 0, 1));
	vk::ImageView msaaColorImageView;
	ret = m_device.createImageView(&msaaColorImageViewInfo, nullptr, &msaaColorImageView);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MSAA Color Image View.\n";
		return false;
	}
	m_msaaColorImageView = msaaColorImageView;
	auto msaaDepthFormant = m_depthFormat;
	auto msaaDepthImageInfo = vk::ImageCreateInfo()
			.setImageType(vk::ImageType::e2D)
			.setFormat(msaaDepthFormant)
			.setExtent({ static_cast<uint32_t>(m_screenWidth), static_cast<uint32_t>(m_screenHeight), 1 })
			.setMipLevels(1)
			.setArrayLayers(1)
			.setSamples(m_msaaSampleCount)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
			.setPQueueFamilyIndices(nullptr)
			.setQueueFamilyIndexCount(0)
			.setSharingMode(vk::SharingMode::eExclusive);
	vk::Image msaaDepthImage;
	ret = m_device.createImage(&msaaDepthImageInfo, nullptr, &msaaDepthImage);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MSAA Depth Image.\n";
		return false;
	}
	m_msaaDepthImage = msaaDepthImage;
	auto msaaDepthMemoReq = m_device.getImageMemoryRequirements(msaaDepthImage);
	uint32_t msaaDepthMemIdx = 0;
	if (!FindMemoryTypeIndex(
		m_memProperties,
		msaaDepthMemoReq.memoryTypeBits,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		&msaaDepthMemIdx)) {
		std::cout << "Failed to find MSAA Depth memory property.\n";
		return false;
	}
	auto msaaDepthMemAllocInfo = vk::MemoryAllocateInfo()
			.setAllocationSize(msaaDepthMemoReq.size)
			.setMemoryTypeIndex(msaaDepthMemIdx);
	vk::DeviceMemory msaaDepthMem;
	ret = m_device.allocateMemory(&msaaDepthMemAllocInfo, nullptr, &msaaDepthMem);
	if (ret != vk::Result::eSuccess) {
		std::cout << "Failed to allocate MSAA Depth memory.\n";
		return false;
	}
	m_msaaDepthMem = msaaDepthMem;
	m_device.bindImageMemory(msaaDepthImage, msaaDepthMem, 0);
	auto msaaDepthImageViewInfo = vk::ImageViewCreateInfo()
			.setImage(msaaDepthImage)
			.setFormat(msaaDepthFormant)
			.setViewType(vk::ImageViewType::e2D)
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth,
			                                               0, 1, 0, 1));
	vk::ImageView msaaDepthImageView;
	ret = m_device.createImageView(&msaaDepthImageViewInfo, nullptr, &msaaDepthImageView);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MSAA Depth Image View.\n";
		return false;
	}
	m_msaaDepthImageView = msaaDepthImageView;
	m_imageIndex = 0;
	return true;
}

bool VKAppContext::PrepareSyncObjects() {
	m_frameSyncDatas.resize(m_imageCount);
	for (auto& [m_fence, m_presentCompleteSemaphore, m_renderCompleteSemaphore] : m_frameSyncDatas) {
		auto semaphoreInfo = vk::SemaphoreCreateInfo();
		vk::Result ret = m_device.createSemaphore(&semaphoreInfo, nullptr, &m_presentCompleteSemaphore);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Present Complete Semaphore.\n";
			return false;
		}
		ret = m_device.createSemaphore(&semaphoreInfo, nullptr, &m_renderCompleteSemaphore);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Render Complete Semaphore.\n";
			return false;
		}
		auto fenceInfo = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);
		ret = m_device.createFence(&fenceInfo, nullptr, &m_fence);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Wait Fence.\n";
			return false;
		}
	}
	return true;
}

bool VKAppContext::PrepareRenderPass() {
	const auto msaaColorAttachment = vk::AttachmentDescription()
			.setFormat(m_colorFormat)
			.setSamples(m_msaaSampleCount)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);
	const auto colorAttachment = vk::AttachmentDescription()
			.setFormat(m_colorFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
	const auto msaaDepthAttachment = vk::AttachmentDescription()
			.setFormat(m_depthFormat)
			.setSamples(m_msaaSampleCount)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setStencilLoadOp(vk::AttachmentLoadOp::eClear)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
	const auto depthAttachment = vk::AttachmentDescription()
			.setFormat(m_depthFormat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setStencilLoadOp(vk::AttachmentLoadOp::eClear)
			.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
	constexpr auto colorRef = vk::AttachmentReference()
			.setAttachment(0)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	constexpr auto depthRef = vk::AttachmentReference()
			.setAttachment(2)
			.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
	constexpr auto resolveRef = vk::AttachmentReference()
			.setAttachment(1)
			.setLayout(vk::ImageLayout::eColorAttachmentOptimal);
	const auto subpass = vk::SubpassDescription()
			.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
			.setInputAttachmentCount(0)
			.setPInputAttachments(nullptr)
			.setColorAttachmentCount(1)
			.setPColorAttachments(&colorRef)
			.setPResolveAttachments(&resolveRef)
			.setPDepthStencilAttachment(&depthRef)
			.setPreserveAttachmentCount(0)
			.setPPreserveAttachments(nullptr);
	vk::SubpassDependency dependencies[2];
	dependencies[0]
			.setSrcSubpass(VK_SUBPASS_EXTERNAL)
			.setDstSubpass(0)
			.setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
			.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead)
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);
	dependencies[1]
			.setSrcSubpass(0)
			.setDstSubpass(VK_SUBPASS_EXTERNAL)
			.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
			.setDstStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eMemoryRead)
			.setDependencyFlags(vk::DependencyFlagBits::eByRegion);
	vk::AttachmentDescription attachments[] = {
		msaaColorAttachment,
		colorAttachment,
		msaaDepthAttachment,
		depthAttachment
	};
	const auto renderPassInfo = vk::RenderPassCreateInfo()
			.setAttachmentCount(std::extent_v<decltype(attachments)>)
			.setPAttachments(attachments)
			.setSubpassCount(1)
			.setPSubpasses(&subpass)
			.setDependencyCount(std::extent_v<decltype(dependencies)>)
			.setPDependencies(dependencies);
	vk::RenderPass renderPass;
	const vk::Result ret = m_device.createRenderPass(&renderPassInfo, nullptr, &renderPass);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Render Pass.\n";
		return false;
	}
	m_renderPass = renderPass;
	return true;
}

bool VKAppContext::PrepareFramebuffer() {
	vk::ImageView attachments[4];
	attachments[0] = m_msaaColorImageView;
	attachments[2] = m_msaaDepthImageView;
	attachments[3] = m_depthImageView;
	const auto framebufferInfo = vk::FramebufferCreateInfo()
			.setRenderPass(m_renderPass)
			.setAttachmentCount(std::extent_v<decltype(attachments)>)
			.setPAttachments(attachments)
			.setWidth(static_cast<uint32_t>(m_screenWidth))
			.setHeight(static_cast<uint32_t>(m_screenHeight))
			.setLayers(1);
	for (auto & m_swapChainImageResource : m_swapChainImageResources) {
		attachments[1] = m_swapChainImageResource.m_imageView;
		vk::Framebuffer framebuffer;
		const vk::Result ret = m_device.createFramebuffer(&framebufferInfo, nullptr, &framebuffer);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Framebuffer.\n";
			return false;
		}
		m_swapChainImageResource.m_framebuffer = framebuffer;
	}
	return true;
}

bool VKAppContext::PreparePipelineCache() {
	constexpr auto pipelineCacheInfo = vk::PipelineCacheCreateInfo();
	const vk::Result ret = m_device.createPipelineCache(&pipelineCacheInfo, nullptr, &m_pipelineCache);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Pipeline Cache.\n";
		return false;
	}
	return true;
}

bool VKAppContext::PrepareMMDPipeline() {
	std::vector<uint32_t> mmd_vert_spv_data, mmd_frag_spv_data;
	if (!ReadSpvBinary(m_shaderDir / "mmd_vert.spv", mmd_vert_spv_data))
		return false;
	if (!ReadSpvBinary(m_shaderDir / "mmd_frag.spv", mmd_frag_spv_data))
		return false;
	vk::Result ret;
	auto vsUniformDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);
	auto fsUniformDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
	auto fsTexDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(2)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
	auto fsToonTexDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(3)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
	auto fsSphereTexDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(4)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
	vk::DescriptorSetLayoutBinding bindings[] = {
		vsUniformDescSetBinding,
		fsUniformDescSetBinding,
		fsTexDescSetBinding,
		fsToonTexDescSetBinding,
		fsSphereTexDescSetBinding,
	};
	auto descSetLayoutInfo = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(std::extent_v<decltype(bindings)>)
			.setPBindings(bindings);
	ret = m_device.createDescriptorSetLayout(&descSetLayoutInfo, nullptr, &m_mmdDescriptorSetLayout);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Descriptor Set Layout.\n";
		return false;
	}
	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
			.setSetLayoutCount(1)
			.setPSetLayouts(&m_mmdDescriptorSetLayout);
	ret = m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_mmdPipelineLayout);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Pipeline Layout.\n";
		return false;
	}
	auto vsInfo = vk::ShaderModuleCreateInfo()
			.setCodeSize(mmd_vert_spv_data.size() * sizeof(uint32_t))
			.setPCode(mmd_vert_spv_data.data());
	ret = m_device.createShaderModule(&vsInfo, nullptr, &m_mmdVSModule);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Vertex Shader Module.\n";
		return false;
	}
	auto fsInfo = vk::ShaderModuleCreateInfo()
			.setCodeSize(mmd_frag_spv_data.size() * sizeof(uint32_t))
			.setPCode(mmd_frag_spv_data.data());
	ret = m_device.createShaderModule(&fsInfo, nullptr, &m_mmdFSModule);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Fragment Shader Module.\n";
		return false;
	}
	auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
			.setLayout(m_mmdPipelineLayout)
			.setRenderPass(m_renderPass);
	auto inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo()
			.setTopology(vk::PrimitiveTopology::eTriangleList);
	pipelineInfo.setPInputAssemblyState(&inputAssemblyInfo);
	auto rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
			.setPolygonMode(vk::PolygonMode::eFill)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setDepthClampEnable(false)
			.setRasterizerDiscardEnable(false)
			.setDepthBiasEnable(false)
			.setLineWidth(1.0f);
	pipelineInfo.setPRasterizationState(&rasterizationInfo);
	auto colorBlendAttachmentInfo = vk::PipelineColorBlendAttachmentState()
			.setColorWriteMask(vk::ColorComponentFlagBits::eR |
			                   vk::ColorComponentFlagBits::eG |
			                   vk::ColorComponentFlagBits::eB |
			                   vk::ColorComponentFlagBits::eA)
			.setBlendEnable(false);
	auto colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo()
			.setAttachmentCount(1)
			.setPAttachments(&colorBlendAttachmentInfo);
	pipelineInfo.setPColorBlendState(&colorBlendStateInfo);
	auto viewportInfo = vk::PipelineViewportStateCreateInfo()
			.setViewportCount(1)
			.setScissorCount(1);
	pipelineInfo.setPViewportState(&viewportInfo);
	vk::DynamicState dynamicStates[2] = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
	auto dynamicInfo = vk::PipelineDynamicStateCreateInfo()
			.setDynamicStateCount(std::extent_v<decltype(dynamicStates)>)
			.setPDynamicStates(dynamicStates);
	pipelineInfo.setPDynamicState(&dynamicInfo);
	auto depthAndStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(true)
			.setDepthWriteEnable(true)
			.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
			.setDepthBoundsTestEnable(false)
			.setBack(vk::StencilOpState()
				.setFailOp(vk::StencilOp::eKeep)
				.setPassOp(vk::StencilOp::eKeep)
				.setCompareOp(vk::CompareOp::eAlways))
			.setStencilTestEnable(false);
	depthAndStencilInfo.front = depthAndStencilInfo.back;
	pipelineInfo.setPDepthStencilState(&depthAndStencilInfo);
	auto multisampleInfo = vk::PipelineMultisampleStateCreateInfo()
			.setRasterizationSamples(m_msaaSampleCount)
			.setSampleShadingEnable(true)
			.setMinSampleShading(0.25f);
	pipelineInfo.setPMultisampleState(&multisampleInfo);
	auto vertexInputBindingDesc = vk::VertexInputBindingDescription()
			.setBinding(0)
			.setStride(sizeof(VKVertex))
			.setInputRate(vk::VertexInputRate::eVertex);
	auto posAttr = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(0)
			.setFormat(vk::Format::eR32G32B32Sfloat)
			.setOffset(offsetof(VKVertex, m_position));
	auto normalAttr = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(1)
			.setFormat(vk::Format::eR32G32B32Sfloat)
			.setOffset(offsetof(VKVertex, m_normal));
	auto uvAttr = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(2)
			.setFormat(vk::Format::eR32G32Sfloat)
			.setOffset(offsetof(VKVertex, m_uv));
	vk::VertexInputAttributeDescription vertexInputAttrs[3] = {
		posAttr,
		normalAttr,
		uvAttr,
	};
	auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
			.setVertexBindingDescriptionCount(1)
			.setPVertexBindingDescriptions(&vertexInputBindingDesc)
			.setVertexAttributeDescriptionCount(std::extent_v<decltype(vertexInputAttrs)>)
			.setPVertexAttributeDescriptions(vertexInputAttrs);
	pipelineInfo.setPVertexInputState(&vertexInputInfo);
	auto vsStageInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(m_mmdVSModule)
			.setPName("main");
	auto fsStageInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(m_mmdFSModule)
			.setPName("main");
	vk::PipelineShaderStageCreateInfo shaderStages[2] = {
		vsStageInfo,
		fsStageInfo,
	};
	pipelineInfo
			.setStageCount(std::extent_v<decltype(shaderStages)>)
			.setPStages(shaderStages);
	colorBlendAttachmentInfo
			.setBlendEnable(true)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
	rasterizationInfo.
			setCullMode(vk::CullModeFlagBits::eBack);
	ret = m_device.createGraphicsPipelines(
		m_pipelineCache,
		1, &pipelineInfo, nullptr,
		&m_mmdPipelines[static_cast<int>(MMDRenderType::AlphaBlend_FrontFace)]);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Pipeline.\n";
		return false;
	}
	rasterizationInfo.setCullMode(vk::CullModeFlagBits::eNone);
	ret = m_device.createGraphicsPipelines(
		m_pipelineCache,
		1, &pipelineInfo, nullptr,
		&m_mmdPipelines[static_cast<int>(MMDRenderType::AlphaBlend_BothFace)]);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Pipeline.\n";
		return false;
	}
	return true;
}

bool VKAppContext::PrepareMMDEdgePipeline() {
	std::vector<uint32_t> mmd_edge_vert_spv_data, mmd_edge_frag_spv_data;
	if (!ReadSpvBinary(m_shaderDir / "mmd_edge_vert.spv", mmd_edge_vert_spv_data))
		return false;
	if (!ReadSpvBinary(m_shaderDir / "mmd_edge_frag.spv", mmd_edge_frag_spv_data))
		return false;
	vk::Result ret;
	auto vsModelUniformDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);
	auto vsMatUniformDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);
	auto fsMatUniformDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(2)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
	vk::DescriptorSetLayoutBinding bindings[] = {
		vsModelUniformDescSetBinding,
		vsMatUniformDescSetBinding,
		fsMatUniformDescSetBinding,
	};
	auto descSetLayoutInfo = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(std::extent_v<decltype(bindings)>)
			.setPBindings(bindings);
	ret = m_device.createDescriptorSetLayout(&descSetLayoutInfo, nullptr, &m_mmdEdgeDescriptorSetLayout);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Edge Descriptor Set Layout.\n";
		return false;
	}
	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
			.setSetLayoutCount(1)
			.setPSetLayouts(&m_mmdEdgeDescriptorSetLayout);
	ret = m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_mmdEdgePipelineLayout);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Edge Pipeline Layout.\n";
		return false;
	}
	auto vsInfo = vk::ShaderModuleCreateInfo()
			.setCodeSize(mmd_edge_vert_spv_data.size() * sizeof(uint32_t))
			.setPCode(mmd_edge_vert_spv_data.data());
	ret = m_device.createShaderModule(&vsInfo, nullptr, &m_mmdEdgeVSModule);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Edge Vertex Shader Module.\n";
		return false;
	}
	auto fsInfo = vk::ShaderModuleCreateInfo()
			.setCodeSize(mmd_edge_frag_spv_data.size() * sizeof(uint32_t))
			.setPCode(mmd_edge_frag_spv_data.data());
	ret = m_device.createShaderModule(&fsInfo, nullptr, &m_mmdEdgeFSModule);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Edge Fragment Shader Module.\n";
		return false;
	}
	auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
			.setLayout(m_mmdEdgePipelineLayout)
			.setRenderPass(m_renderPass);
	auto inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo()
			.setTopology(vk::PrimitiveTopology::eTriangleList);
	pipelineInfo.setPInputAssemblyState(&inputAssemblyInfo);
	auto rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
			.setPolygonMode(vk::PolygonMode::eFill)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setDepthClampEnable(false)
			.setRasterizerDiscardEnable(false)
			.setDepthBiasEnable(false)
			.setLineWidth(1.0f);
	pipelineInfo.setPRasterizationState(&rasterizationInfo);
	auto colorBlendAttachmentInfo = vk::PipelineColorBlendAttachmentState()
			.setColorWriteMask(vk::ColorComponentFlagBits::eR |
			                   vk::ColorComponentFlagBits::eG |
			                   vk::ColorComponentFlagBits::eB |
			                   vk::ColorComponentFlagBits::eA)
			.setBlendEnable(false);
	auto colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo()
			.setAttachmentCount(1)
			.setPAttachments(&colorBlendAttachmentInfo);
	pipelineInfo.setPColorBlendState(&colorBlendStateInfo);
	auto viewportInfo = vk::PipelineViewportStateCreateInfo()
			.setViewportCount(1)
			.setScissorCount(1);
	pipelineInfo.setPViewportState(&viewportInfo);
	vk::DynamicState dynamicStates[2] = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};
	auto dynamicInfo = vk::PipelineDynamicStateCreateInfo()
			.setDynamicStateCount(std::extent_v<decltype(dynamicStates)>)
			.setPDynamicStates(dynamicStates);
	pipelineInfo.setPDynamicState(&dynamicInfo);
	auto depthAndStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(true)
			.setDepthWriteEnable(true)
			.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
			.setDepthBoundsTestEnable(false)
			.setBack(vk::StencilOpState()
				.setFailOp(vk::StencilOp::eKeep)
				.setPassOp(vk::StencilOp::eKeep)
				.setCompareOp(vk::CompareOp::eAlways))
			.setStencilTestEnable(false);
	depthAndStencilInfo.front = depthAndStencilInfo.back;
	pipelineInfo.setPDepthStencilState(&depthAndStencilInfo);
	auto multisampleInfo = vk::PipelineMultisampleStateCreateInfo()
			.setRasterizationSamples(m_msaaSampleCount)
			.setSampleShadingEnable(true)
			.setMinSampleShading(0.25f);
	pipelineInfo.setPMultisampleState(&multisampleInfo);
	auto vertexInputBindingDesc = vk::VertexInputBindingDescription()
			.setBinding(0)
			.setStride(sizeof(VKVertex))
			.setInputRate(vk::VertexInputRate::eVertex);
	auto posAttr = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(0)
			.setFormat(vk::Format::eR32G32B32Sfloat)
			.setOffset(offsetof(VKVertex, m_position));
	auto normalAttr = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(1)
			.setFormat(vk::Format::eR32G32B32Sfloat)
			.setOffset(offsetof(VKVertex, m_normal));
	vk::VertexInputAttributeDescription vertexInputAttrs[] = {
		posAttr,
		normalAttr,
	};
	auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
			.setVertexBindingDescriptionCount(1)
			.setPVertexBindingDescriptions(&vertexInputBindingDesc)
			.setVertexAttributeDescriptionCount(std::extent<decltype(vertexInputAttrs)>::value)
			.setPVertexAttributeDescriptions(vertexInputAttrs);
	pipelineInfo.setPVertexInputState(&vertexInputInfo);
	auto vsStageInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(m_mmdEdgeVSModule)
			.setPName("main");
	auto fsStageInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(m_mmdEdgeFSModule)
			.setPName("main");
	vk::PipelineShaderStageCreateInfo shaderStages[2] = {
		vsStageInfo,
		fsStageInfo,
	};
	pipelineInfo
			.setStageCount(std::extent_v<decltype(shaderStages)>)
			.setPStages(shaderStages);
	colorBlendAttachmentInfo
			.setBlendEnable(true)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
	rasterizationInfo.
			setCullMode(vk::CullModeFlagBits::eFront);
	ret = m_device.createGraphicsPipelines(
		m_pipelineCache,
		1, &pipelineInfo, nullptr,
		&m_mmdEdgePipeline);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Edge Pipeline.\n";
		return false;
	}
	return true;
}

bool VKAppContext::PrepareMMDGroundShadowPipeline() {
	std::vector<uint32_t> mmd_ground_shadow_vert_spv_data, mmd_ground_shadow_frag_spv_data;
	if (!ReadSpvBinary(m_shaderDir / "mmd_gs_vert.spv", mmd_ground_shadow_vert_spv_data))
		return false;
	if (!ReadSpvBinary(m_shaderDir / "mmd_gs_frag.spv", mmd_ground_shadow_frag_spv_data))
		return false;
	vk::Result ret;
	auto vsModelUniformDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(0)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);
	auto fsMatUniformDescSetBinding = vk::DescriptorSetLayoutBinding()
			.setBinding(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(1)
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);
	vk::DescriptorSetLayoutBinding bindings[] = {
		vsModelUniformDescSetBinding,
		fsMatUniformDescSetBinding,
	};
	auto descSetLayoutInfo = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(std::extent_v<decltype(bindings)>)
			.setPBindings(bindings);
	ret = m_device.createDescriptorSetLayout(&descSetLayoutInfo, nullptr, &m_mmdGroundShadowDescriptorSetLayout);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Ground Shadow Descriptor Set Layout.\n";
		return false;
	}
	auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
			.setSetLayoutCount(1)
			.setPSetLayouts(&m_mmdGroundShadowDescriptorSetLayout);
	ret = m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_mmdGroundShadowPipelineLayout);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Ground Shadow Pipeline Layout.\n";
		return false;
	}
	auto vsInfo = vk::ShaderModuleCreateInfo()
			.setCodeSize(mmd_ground_shadow_vert_spv_data.size() * sizeof(uint32_t))
			.setPCode(mmd_ground_shadow_vert_spv_data.data());
	ret = m_device.createShaderModule(&vsInfo, nullptr, &m_mmdGroundShadowVSModule);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Ground Shadow Vertex Shader Module.\n";
		return false;
	}
	auto fsInfo = vk::ShaderModuleCreateInfo()
			.setCodeSize(mmd_ground_shadow_frag_spv_data.size() * sizeof(uint32_t))
			.setPCode(mmd_ground_shadow_frag_spv_data.data());
	ret = m_device.createShaderModule(&fsInfo, nullptr, &m_mmdGroundShadowFSModule);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD GroundShadow Fragment Shader Module.\n";
		return false;
	}
	auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
			.setLayout(m_mmdGroundShadowPipelineLayout)
			.setRenderPass(m_renderPass);
	auto inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo()
			.setTopology(vk::PrimitiveTopology::eTriangleList);
	pipelineInfo.setPInputAssemblyState(&inputAssemblyInfo);
	auto rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
			.setPolygonMode(vk::PolygonMode::eFill)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setDepthClampEnable(false)
			.setRasterizerDiscardEnable(false)
			.setDepthBiasEnable(false)
			.setLineWidth(1.0f);
	pipelineInfo.setPRasterizationState(&rasterizationInfo);
	auto colorBlendAttachmentInfo = vk::PipelineColorBlendAttachmentState()
			.setColorWriteMask(vk::ColorComponentFlagBits::eR |
			                   vk::ColorComponentFlagBits::eG |
			                   vk::ColorComponentFlagBits::eB |
			                   vk::ColorComponentFlagBits::eA)
			.setBlendEnable(false);
	auto colorBlendStateInfo = vk::PipelineColorBlendStateCreateInfo()
			.setAttachmentCount(1)
			.setPAttachments(&colorBlendAttachmentInfo);
	pipelineInfo.setPColorBlendState(&colorBlendStateInfo);
	auto viewportInfo = vk::PipelineViewportStateCreateInfo()
			.setViewportCount(1)
			.setScissorCount(1);
	pipelineInfo.setPViewportState(&viewportInfo);
	vk::DynamicState dynamicStates[] = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor,
		vk::DynamicState::eDepthBias,
		vk::DynamicState::eStencilReference,
		vk::DynamicState::eStencilCompareMask,
		vk::DynamicState::eStencilWriteMask,
	};
	auto dynamicInfo = vk::PipelineDynamicStateCreateInfo()
			.setDynamicStateCount(std::extent_v<decltype(dynamicStates)>)
			.setPDynamicStates(dynamicStates);
	pipelineInfo.setPDynamicState(&dynamicInfo);
	auto depthAndStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
			.setDepthTestEnable(true)
			.setDepthWriteEnable(true)
			.setDepthCompareOp(vk::CompareOp::eLessOrEqual)
			.setDepthBoundsTestEnable(false)
			.setFront(vk::StencilOpState()
				.setCompareOp(vk::CompareOp::eNotEqual)
				.setFailOp(vk::StencilOp::eKeep)
				.setDepthFailOp(vk::StencilOp::eKeep)
				.setPassOp(vk::StencilOp::eReplace)
			)
			.setBack(vk::StencilOpState()
				.setCompareOp(vk::CompareOp::eNotEqual)
				.setFailOp(vk::StencilOp::eKeep)
				.setDepthFailOp(vk::StencilOp::eKeep)
				.setPassOp(vk::StencilOp::eReplace))
			.setStencilTestEnable(true);
	depthAndStencilInfo.front = depthAndStencilInfo.back;
	pipelineInfo.setPDepthStencilState(&depthAndStencilInfo);
	auto multisampleInfo = vk::PipelineMultisampleStateCreateInfo()
			.setRasterizationSamples(m_msaaSampleCount)
			.setSampleShadingEnable(true)
			.setMinSampleShading(0.25f);
	pipelineInfo.setPMultisampleState(&multisampleInfo);
	auto vertexInputBindingDesc = vk::VertexInputBindingDescription()
			.setBinding(0)
			.setStride(sizeof(VKVertex))
			.setInputRate(vk::VertexInputRate::eVertex);
	auto posAttr = vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(0)
			.setFormat(vk::Format::eR32G32B32Sfloat)
			.setOffset(offsetof(VKVertex, m_position));
	vk::VertexInputAttributeDescription vertexInputAttrs[] = {
		posAttr,
	};
	auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
			.setVertexBindingDescriptionCount(1)
			.setPVertexBindingDescriptions(&vertexInputBindingDesc)
			.setVertexAttributeDescriptionCount(std::extent_v<decltype(vertexInputAttrs)>)
			.setPVertexAttributeDescriptions(vertexInputAttrs);
	pipelineInfo.setPVertexInputState(&vertexInputInfo);
	auto vsStageInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(m_mmdGroundShadowVSModule)
			.setPName("main");
	auto fsStageInfo = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(m_mmdGroundShadowFSModule)
			.setPName("main");
	vk::PipelineShaderStageCreateInfo shaderStages[2] = {
		vsStageInfo,
		fsStageInfo,
	};
	pipelineInfo
			.setStageCount(std::extent_v<decltype(shaderStages)>)
			.setPStages(shaderStages);
	colorBlendAttachmentInfo
			.setBlendEnable(true)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setAlphaBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
	rasterizationInfo.
			setCullMode(vk::CullModeFlagBits::eNone);
	ret = m_device.createGraphicsPipelines(
		m_pipelineCache,
		1, &pipelineInfo, nullptr,
		&m_mmdGroundShadowPipeline);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create MMD Ground Shadow Pipeline.\n";
		return false;
	}
	return true;
}

bool VKAppContext::PrepareDefaultTexture() {
	m_defaultTexture = std::make_unique<VKTexture>();
	if (!m_defaultTexture->Setup(*this, 1, 1, vk::Format::eR8G8B8A8Unorm))
		return false;
	VKStagingBuffer* imgStBuf;
	constexpr uint32_t x = 1;
	constexpr uint32_t y = 1;
	constexpr uint32_t memSize = x * y * 4;
	if (!GetStagingBuffer(memSize, &imgStBuf))
		return false;
	void* imgPtr;
	const vk::Result res = m_device.mapMemory(imgStBuf->m_memory, 0, memSize, {}, &imgPtr);
	if (res != vk::Result::eSuccess || !imgPtr)
		return false;
	const auto pixels = static_cast<uint8_t*>(imgPtr);
	pixels[0] = 0;
	pixels[0] = 0;
	pixels[0] = 0;
	pixels[0] = 255;
	m_device.unmapMemory(imgStBuf->m_memory);
	constexpr auto bufferImageCopy = vk::BufferImageCopy()
			.setImageSubresource(vk::ImageSubresourceLayers()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setMipLevel(0)
				.setBaseArrayLayer(0)
				.setLayerCount(1))
			.setImageExtent(vk::Extent3D(x, y, 1))
			.setBufferOffset(0);
	if (!imgStBuf->CopyImage(
		*this,
		m_defaultTexture->m_image,
		vk::ImageLayout::eShaderReadOnlyOptimal,
		1, &bufferImageCopy)) {
		std::cout << "Failed to copy image.\n";
		return false;
	}
	m_defaultTexture->m_imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	m_defaultTexture->m_hasAlpha = true;
	return true;
}

bool VKAppContext::Resize() {
	m_device.waitIdle();
	if (!PrepareBuffer())
		return false;
	if (!PrepareFramebuffer())
		return false;
	return true;
}

bool VKAppContext::GetStagingBuffer(const vk::DeviceSize memSize, VKStagingBuffer** outBuf) {
	if (outBuf == nullptr)
		return false;
	vk::Result ret;
	for (const auto& stBuf : m_stagingBuffers) {
		ret = m_device.getFenceStatus(stBuf->m_transferCompleteFence);
		if (vk::Result::eSuccess == ret && memSize < stBuf->m_memorySize) {
			const vk::Result res = m_device.resetFences(1, &stBuf->m_transferCompleteFence);
			if (res != vk::Result::eSuccess)
				return false;
			*outBuf = stBuf.get();
			return true;
		}
	}
	for (const auto& stBuf : m_stagingBuffers) {
		ret = m_device.getFenceStatus(stBuf->m_transferCompleteFence);
		if (vk::Result::eSuccess == ret) {
			if (!stBuf->Setup(*this, memSize)) {
				std::cout << "Failed to setup Staging Buffer.\n";
				return false;
			}
			const vk::Result res = m_device.resetFences(1, &stBuf->m_transferCompleteFence);
			if (res != vk::Result::eSuccess)
				return false;
			*outBuf = stBuf.get();
			return true;
		}
	}
	auto newStagingBuffer = std::make_unique<VKStagingBuffer>();
	if (!newStagingBuffer->Setup(*this, memSize)) {
		std::cout << "Failed to setup Staging Buffer.\n";
		newStagingBuffer->Clear(*this);
		return false;
	}
	const vk::Result res = m_device.resetFences(1, &newStagingBuffer->m_transferCompleteFence);
	if (res != vk::Result::eSuccess)
		return false;
	*outBuf = newStagingBuffer.get();
	m_stagingBuffers.emplace_back(std::move(newStagingBuffer));
	return true;
}

void VKAppContext::WaitAllStagingBuffer() const {
	for (const auto& stBuf : m_stagingBuffers) {
		const vk::Result res = m_device.waitForFences(1, &stBuf->m_transferCompleteFence, true, UINT64_MAX);
		if (res != vk::Result::eSuccess)
			std::cout << "Failed to wait for Staging Buffer.\n";
	}
}

bool VKAppContext::GetTexture(const std::filesystem::path& texturePath, VKTexture** outTex) {
	const auto it = m_textures.find(texturePath);
	if (it == m_textures.end()) {
		FILE* fp = nullptr;
		if (_wfopen_s(&fp, texturePath.c_str(), L"rb") != 0 || !fp)
			return false;
		int x = 0, y = 0, comp = 0;
		stbi_uc* image = stbi_load_from_file(fp, &x, &y, &comp, STBI_rgb_alpha);
		std::fclose(fp);
		if (!image)
			return false;
		const bool hasAlpha = comp == 4;
		constexpr auto format = vk::Format::eR8G8B8A8Unorm;
		auto tex = std::make_unique<VKTexture>();
		if (!tex->Setup(*this, x, y, format)) {
			stbi_image_free(image);
			return false;
		}
		const uint32_t memSize = x * y * 4;
		VKStagingBuffer* imgStBuf;
		if (!GetStagingBuffer(memSize, &imgStBuf)) {
			stbi_image_free(image);
			return false;
		}
		void* imgPtr;
		const vk::Result res = m_device.mapMemory(imgStBuf->m_memory, 0, memSize, vk::MemoryMapFlags(), &imgPtr);
		if (res != vk::Result::eSuccess) {
			stbi_image_free(image);
			return false;
		}
		memcpy(imgPtr, image, memSize);
		m_device.unmapMemory(imgStBuf->m_memory);
		stbi_image_free(image);
		const auto bufferImageCopy = vk::BufferImageCopy()
				.setImageSubresource(vk::ImageSubresourceLayers()
					.setAspectMask(vk::ImageAspectFlagBits::eColor)
					.setMipLevel(0)
					.setBaseArrayLayer(0)
					.setLayerCount(1))
				.setImageExtent(vk::Extent3D(x, y, 1))
				.setBufferOffset(0);
		if (!imgStBuf->CopyImage(
			*this,
			tex->m_image,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			1, &bufferImageCopy)) {
			std::cout << "Failed to copy image.\n";
			return false;
		}
		tex->m_imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		tex->m_hasAlpha = hasAlpha;
		*outTex = tex.get();
		m_textures[texturePath] = std::move(tex);
	} else
		*outTex = it->second.get();
	return true;
}

bool VKAppContext::ReadSpvBinary(const std::filesystem::path& path, std::vector<uint32_t>& out) {
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f)
		return false;
	const std::streamsize size = f.tellg();
	if (size < 0)
		return false;
	if (size % 4 != 0)
		return false;
	f.seekg(0, std::ios::beg);
	out.resize(static_cast<size_t>(size / 4));
	if (!f.read(reinterpret_cast<char*>(out.data()), size))
		return false;
	return true;
}

bool Model::Setup(VKAppContext& appContext) {
	const auto device = appContext.m_device;
	const size_t matCount = m_mmdModel->m_materials.size();
	m_materials.resize(matCount);
	for (size_t i = 0; i < matCount; i++) {
		const auto& mmdMat = m_mmdModel->m_materials[i];
		auto& [m_mmdTex, m_mmdTexSampler, m_mmdToonTex
			, m_mmdToonTexSampler, m_mmdSphereTex, m_mmdSphereTexSampler] = m_materials[i];
		if (!mmdMat.m_texture.empty()) {
			if (!appContext.GetTexture(mmdMat.m_texture, &m_mmdTex)) {
				std::cout << "Failed to get Texture.\n";
				return false;
			}
		} else
			m_mmdTex = appContext.m_defaultTexture.get();
		auto samplerInfo = vk::SamplerCreateInfo()
				.setMagFilter(vk::Filter::eLinear)
				.setMinFilter(vk::Filter::eLinear)
				.setMipmapMode(vk::SamplerMipmapMode::eLinear)
				.setAddressModeU(vk::SamplerAddressMode::eRepeat)
				.setAddressModeV(vk::SamplerAddressMode::eRepeat)
				.setAddressModeW(vk::SamplerAddressMode::eRepeat)
				.setMipLodBias(0.0f)
				.setCompareOp(vk::CompareOp::eNever)
				.setMinLod(0.0f)
				.setMaxLod(static_cast<float>(m_mmdTex->m_mipLevel))
				.setMaxAnisotropy(1.0f)
				.setAnisotropyEnable(false);
		vk::Result ret = device.createSampler(&samplerInfo, nullptr, &m_mmdTexSampler);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Sampler.\n";
			return false;
		}
		if (!mmdMat.m_toonTexture.empty()) {
			if (!appContext.GetTexture(mmdMat.m_toonTexture, &m_mmdToonTex)) {
				std::cout << "Failed to get Toon Texture.\n";
				return false;
			}
		} else
			m_mmdToonTex = appContext.m_defaultTexture.get();
		auto toonSamplerInfo = vk::SamplerCreateInfo()
				.setMagFilter(vk::Filter::eLinear)
				.setMinFilter(vk::Filter::eLinear)
				.setMipmapMode(vk::SamplerMipmapMode::eLinear)
				.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
				.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
				.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
				.setMipLodBias(0.0f)
				.setCompareOp(vk::CompareOp::eNever)
				.setMinLod(0.0f)
				.setMaxLod(static_cast<float>(m_mmdToonTex->m_mipLevel))
				.setMaxAnisotropy(1.0f)
				.setAnisotropyEnable(false);
		ret = device.createSampler(&toonSamplerInfo, nullptr, &m_mmdToonTexSampler);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Sampler.\n";
			return false;
		}
		if (!mmdMat.m_spTexture.empty()) {
			if (!appContext.GetTexture(mmdMat.m_spTexture, &m_mmdSphereTex)) {
				std::cout << "Failed to get Sphere Texture.\n";
				return false;
			}
		} else
			m_mmdSphereTex = appContext.m_defaultTexture.get();
		auto sphereSamplerInfo = vk::SamplerCreateInfo()
				.setMagFilter(vk::Filter::eLinear)
				.setMinFilter(vk::Filter::eLinear)
				.setMipmapMode(vk::SamplerMipmapMode::eLinear)
				.setAddressModeU(vk::SamplerAddressMode::eRepeat)
				.setAddressModeV(vk::SamplerAddressMode::eRepeat)
				.setAddressModeW(vk::SamplerAddressMode::eRepeat)
				.setMipLodBias(0.0f)
				.setCompareOp(vk::CompareOp::eNever)
				.setMinLod(0.0f)
				.setMaxLod(static_cast<float>(m_mmdSphereTex->m_mipLevel))
				.setMaxAnisotropy(1.0f)
				.setAnisotropyEnable(false);
		ret = device.createSampler(&sphereSamplerInfo, nullptr, &m_mmdSphereTexSampler);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to create Sampler.\n";
			return false;
		}
	}
	if (!SetupVertexBuffer(appContext)) return false;
	if (!SetupDescriptorPool(appContext)) return false;
	if (!SetupDescriptorSet(appContext)) return false;
	if (!SetupCommandBuffer(appContext)) return false;
	return true;
}

bool Model::SetupVertexBuffer(VKAppContext& appContext) {
	const auto device = appContext.m_device;
	auto& [m_modelResource, m_materialResources] = m_resource;
	auto& modelRes = m_modelResource;
	auto& vb = modelRes.m_vertexBuffer;
	const auto vbMemSize = static_cast<uint32_t>(sizeof(VKVertex) * m_mmdModel->m_positions.size());
	if (!vb.Setup(appContext, vbMemSize,
		vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst)) {
		std::cout << "Failed to create Vertex Buffer.\n";
		return false;
	}
	const auto ibMemSize = static_cast<uint32_t>(m_mmdModel->m_indexElementSize * m_mmdModel->m_indexCount);
	if (!m_indexBuffer.Setup(appContext, ibMemSize,
		vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst)) {
		std::cout << "Failed to create Index Buffer.\n";
		return false;
	}
	VKStagingBuffer* indicesStagingBuffer;
	if (!appContext.GetStagingBuffer(ibMemSize, &indicesStagingBuffer)) {
		std::cout << "Failed to get Staging Buffer.\n";
		return false;
	}
	void* mapMem;
	const vk::Result ret = device.mapMemory(indicesStagingBuffer->m_memory, 0, ibMemSize,
		static_cast<vk::MemoryMapFlagBits>(0), &mapMem);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to map memory.\n";
		return false;
	}
	std::memcpy(mapMem, &m_mmdModel->m_indices[0], ibMemSize);
	device.unmapMemory(indicesStagingBuffer->m_memory);
	if (!indicesStagingBuffer->CopyBuffer(appContext, m_indexBuffer.m_buffer, ibMemSize)) {
		std::cout << "Failed to copy buffer.\n";
		return false;
	}
	indicesStagingBuffer->Wait(appContext);
	if (m_mmdModel->m_indexElementSize == 1) {
		std::cout << "Vulkan is not supported uint8_t index.\n";
		return false;
	}
	if (m_mmdModel->m_indexElementSize == 2)
		m_indexType = vk::IndexType::eUint16;
	else if (m_mmdModel->m_indexElementSize == 4)
		m_indexType = vk::IndexType::eUint32;
	else {
		std::cout << "Unknown index size.[" << m_mmdModel->m_indexElementSize << "]\n";
		return false;
	}
	return true;
}

bool Model::SetupDescriptorPool(const VKAppContext& appContext) {
	const auto device = appContext.m_device;
	const auto matCount = static_cast<uint32_t>(m_mmdModel->m_materials.size());
	uint32_t ubCount = 7;
	ubCount *= matCount;
	const auto ubPoolSize = vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(ubCount);
	uint32_t imgPoolCount = 3;
	imgPoolCount *= matCount;
	const auto imgPoolSize = vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(imgPoolCount);
	vk::DescriptorPoolSize poolSizes[] = {
		ubPoolSize,
		imgPoolSize
	};
	uint32_t descSetCount = 3;
	descSetCount *= matCount;
	const auto descPoolInfo = vk::DescriptorPoolCreateInfo()
			.setMaxSets(descSetCount)
			.setPoolSizeCount(std::extent_v<decltype(poolSizes)>)
			.setPPoolSizes(poolSizes);
	const vk::Result ret = device.createDescriptorPool(&descPoolInfo, nullptr, &m_descPool);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to create Descriptor Pool.\n";
		return false;
	}
	return true;
}

bool Model::SetupDescriptorSet(VKAppContext& appContext) {
	vk::Result ret;
	auto device = appContext.m_device;
	auto mmdDescSetAllocInfo = vk::DescriptorSetAllocateInfo()
			.setDescriptorPool(m_descPool)
			.setDescriptorSetCount(1)
			.setPSetLayouts(&appContext.m_mmdDescriptorSetLayout);
	auto mmdEdgeDescSetAllocInfo = vk::DescriptorSetAllocateInfo()
			.setDescriptorPool(m_descPool)
			.setDescriptorSetCount(1)
			.setPSetLayouts(&appContext.m_mmdEdgeDescriptorSetLayout);
	auto mmdGroundShadowDescSetAllocInfo = vk::DescriptorSetAllocateInfo()
			.setDescriptorPool(m_descPool)
			.setDescriptorSetCount(1)
			.setPSetLayouts(&appContext.m_mmdGroundShadowDescriptorSetLayout);
	auto gpu = appContext.m_gpu;
	auto gpuProp = gpu.getProperties();
	auto ubAlign = static_cast<uint32_t>(gpuProp.limits.minUniformBufferOffsetAlignment);
	uint32_t ubOffset = 0;
	auto& [m_modelResource1, m_materialResources1] = m_resource;
	auto& modelRes1 = m_modelResource1;
	modelRes1.m_mmdVSUBOffset = ubOffset;
	ubOffset += sizeof(VKVertxShaderUB);
	ubOffset = ubOffset + ubAlign - (ubOffset + ubAlign) % ubAlign;
	modelRes1.m_mmdEdgeVSUBOffset = ubOffset;
	ubOffset += sizeof(VKEdgeVertexShaderUB);
	ubOffset = ubOffset + ubAlign - (ubOffset + ubAlign) % ubAlign;
	modelRes1.m_mmdGroundShadowVSUBOffset = ubOffset;
	ubOffset += sizeof(VKGroundShadowVertexShaderUB);
	ubOffset = ubOffset + ubAlign - (ubOffset + ubAlign) % ubAlign;
	size_t matCount = m_mmdModel->m_materials.size();
	m_materialResources1.resize(matCount);
	for (size_t matIdx = 0; matIdx < matCount; matIdx++) {
		auto& [m_mmdDescSet, m_mmdEdgeDescSet, m_mmdGroundShadowDescSet
			, m_mmdFSUBOffset, m_mmdEdgeSizeVSUBOffset
			, m_mmdEdgeFSUBOffset, m_mmdGroundShadowFSUBOffset] = m_materialResources1[matIdx];
		ret = device.allocateDescriptorSets(&mmdDescSetAllocInfo, &m_mmdDescSet);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to allocate MMD Descriptor Set.\n";
			return false;
		}
		ret = device.allocateDescriptorSets(&mmdEdgeDescSetAllocInfo, &m_mmdEdgeDescSet);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to allocate MMD Edge Descriptor Set.\n";
			return false;
		}
		ret = device.allocateDescriptorSets(&mmdGroundShadowDescSetAllocInfo, &m_mmdGroundShadowDescSet);
		if (vk::Result::eSuccess != ret) {
			std::cout << "Failed to allocate MMD Ground Shadow Descriptor Set.\n";
			return false;
		}
		m_mmdFSUBOffset = ubOffset;
		ubOffset += sizeof(VKFragmentShaderUB);
		ubOffset = ubOffset + ubAlign - (ubOffset + ubAlign) % ubAlign;
		m_mmdEdgeSizeVSUBOffset = ubOffset;
		ubOffset += sizeof(VKEdgeSizeVertexShaderUB);
		ubOffset = ubOffset + ubAlign - (ubOffset + ubAlign) % ubAlign;
		m_mmdEdgeFSUBOffset = ubOffset;
		ubOffset += sizeof(VKEdgeFragmentShaderUB);
		ubOffset = ubOffset + ubAlign - (ubOffset + ubAlign) % ubAlign;
		m_mmdGroundShadowFSUBOffset = ubOffset;
		ubOffset += sizeof(VKGroundShadowFragmentShaderUB);
		ubOffset = ubOffset + ubAlign - (ubOffset + ubAlign) % ubAlign;
	}
	auto& ub = modelRes1.m_uniformBuffer;
	auto ubMemSize = ubOffset;
	if (!ub.Setup(
		appContext,
		ubMemSize,
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst
	)) {
		std::cout << "Failed to create Uniform Buffer.\n";
		return false;
	}
	auto mmdVSBufferInfo = vk::DescriptorBufferInfo()
			.setOffset(0)
			.setRange(sizeof(VKVertxShaderUB));
	auto mmdVSWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(0)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&mmdVSBufferInfo);
	auto mmdFSBufferInfo = vk::DescriptorBufferInfo()
			.setOffset(0)
			.setRange(sizeof(VKFragmentShaderUB));
	auto mmdFSWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(1)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&mmdFSBufferInfo);
	auto mmdFSTexSamplerInfo = vk::DescriptorImageInfo();
	auto mmdFSTexSamplerWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(2)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setPImageInfo(&mmdFSTexSamplerInfo);
	auto mmdFSToonTexSamplerInfo = vk::DescriptorImageInfo();
	auto mmdFSToonTexSamplerWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(3)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setPImageInfo(&mmdFSToonTexSamplerInfo);
	auto mmdFSSphereTexSamplerInfo = vk::DescriptorImageInfo();
	auto mmdFSSphereTexSamplerWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(4)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setPImageInfo(&mmdFSSphereTexSamplerInfo);
	vk::WriteDescriptorSet mmdWriteDescSets[] = {
		mmdVSWriteDescSet,
		mmdFSWriteDescSet,
		mmdFSTexSamplerWriteDescSet,
		mmdFSToonTexSamplerWriteDescSet,
		mmdFSSphereTexSamplerWriteDescSet,
	};
	auto mmdEdgeVSBufferInfo = vk::DescriptorBufferInfo()
			.setOffset(0)
			.setRange(sizeof(VKEdgeVertexShaderUB));
	auto mmdEdgeVSWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(0)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&mmdEdgeVSBufferInfo);
	auto mmdEdgeSizeVSBufferInfo = vk::DescriptorBufferInfo()
			.setOffset(0)
			.setRange(sizeof(VKEdgeSizeVertexShaderUB));
	auto mmdEdgeSizeVSWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(1)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&mmdEdgeSizeVSBufferInfo);
	auto mmdEdgeFSBufferInfo = vk::DescriptorBufferInfo()
			.setOffset(0)
			.setRange(sizeof(VKEdgeFragmentShaderUB));
	auto mmdEdgeFSWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(2)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&mmdEdgeFSBufferInfo);
	vk::WriteDescriptorSet mmdEdgeWriteDescSets[] = {
		mmdEdgeVSWriteDescSet,
		mmdEdgeSizeVSWriteDescSet,
		mmdEdgeFSWriteDescSet,
	};
	auto mmdGroundShadowVSBufferInfo = vk::DescriptorBufferInfo()
			.setOffset(0)
			.setRange(sizeof(VKGroundShadowVertexShaderUB));
	auto mmdGroundShadowVSWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(0)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&mmdGroundShadowVSBufferInfo);
	auto mmdGroundShadowFSBufferInfo = vk::DescriptorBufferInfo()
			.setOffset(0)
			.setRange(sizeof(VKGroundShadowFragmentShaderUB));
	auto mmdGroundShadowFSWriteDescSet = vk::WriteDescriptorSet()
			.setDstBinding(1)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setPBufferInfo(&mmdGroundShadowFSBufferInfo);
	vk::WriteDescriptorSet mmdGroundShadowWriteDescSets[] = {
		mmdGroundShadowVSWriteDescSet,
		mmdGroundShadowFSWriteDescSet,
	};
	auto& [m_modelResource2, m_materialResources2] = m_resource;
	auto& modelRes2 = m_modelResource2;
	mmdVSBufferInfo.setBuffer(modelRes2.m_uniformBuffer.m_buffer);
	mmdVSBufferInfo.setOffset(modelRes2.m_mmdVSUBOffset);
	mmdEdgeVSBufferInfo.setBuffer(modelRes2.m_uniformBuffer.m_buffer);
	mmdEdgeVSBufferInfo.setOffset(modelRes2.m_mmdEdgeVSUBOffset);
	mmdGroundShadowVSBufferInfo.setBuffer(modelRes2.m_uniformBuffer.m_buffer);
	mmdGroundShadowVSBufferInfo.setOffset(modelRes2.m_mmdGroundShadowVSUBOffset);
	size_t matCount2 = m_mmdModel->m_materials.size();
	m_materialResources2.resize(matCount2);
	for (size_t matIdx = 0; matIdx < matCount2; matIdx++) {
		auto& [m_mmdDescSet, m_mmdEdgeDescSet, m_mmdGroundShadowDescSet
			, m_mmdFSUBOffset, m_mmdEdgeSizeVSUBOffset
			, m_mmdEdgeFSUBOffset, m_mmdGroundShadowFSUBOffset] = m_materialResources2[matIdx];
		const auto& [m_mmdTex, m_mmdTexSampler, m_mmdToonTex
			, m_mmdToonTexSampler, m_mmdSphereTex, m_mmdSphereTexSampler] = m_materials[matIdx];
		mmdFSBufferInfo.setBuffer(modelRes2.m_uniformBuffer.m_buffer);
		mmdFSBufferInfo.setOffset(m_mmdFSUBOffset);
		mmdFSTexSamplerInfo.setImageView(m_mmdTex->m_imageView);
		mmdFSTexSamplerInfo.setImageLayout(m_mmdTex->m_imageLayout);
		mmdFSTexSamplerInfo.setSampler(m_mmdTexSampler);
		mmdFSToonTexSamplerInfo.setImageView(m_mmdToonTex->m_imageView);
		mmdFSToonTexSamplerInfo.setImageLayout(m_mmdToonTex->m_imageLayout);
		mmdFSToonTexSamplerInfo.setSampler(m_mmdToonTexSampler);
		mmdFSSphereTexSamplerInfo.setImageView(m_mmdSphereTex->m_imageView);
		mmdFSSphereTexSamplerInfo.setImageLayout(m_mmdSphereTex->m_imageLayout);
		mmdFSSphereTexSamplerInfo.setSampler(m_mmdSphereTexSampler);
		uint32_t writesCount = std::extent<decltype(mmdWriteDescSets)>::value;
		for (uint32_t i = 0; i < writesCount; i++)
			mmdWriteDescSets[i].setDstSet(m_mmdDescSet);
		device.updateDescriptorSets(writesCount, mmdWriteDescSets,
		                            0, nullptr);
		mmdEdgeSizeVSBufferInfo.setBuffer(modelRes2.m_uniformBuffer.m_buffer);
		mmdEdgeSizeVSBufferInfo.setOffset(m_mmdEdgeSizeVSUBOffset);
		mmdEdgeFSBufferInfo.setBuffer(modelRes2.m_uniformBuffer.m_buffer);
		mmdEdgeFSBufferInfo.setOffset(m_mmdEdgeFSUBOffset);
		writesCount = std::extent_v<decltype(mmdEdgeWriteDescSets)>;
		for (uint32_t i = 0; i < writesCount; i++)
			mmdEdgeWriteDescSets[i].setDstSet(m_mmdEdgeDescSet);
		device.updateDescriptorSets(writesCount, mmdEdgeWriteDescSets,
		                            0, nullptr);
		mmdGroundShadowFSBufferInfo.setBuffer(modelRes2.m_uniformBuffer.m_buffer);
		mmdGroundShadowFSBufferInfo.setOffset(m_mmdGroundShadowFSUBOffset);
		writesCount = std::extent_v<decltype(mmdGroundShadowWriteDescSets)>;
		for (uint32_t i = 0; i < writesCount; i++)
			mmdGroundShadowWriteDescSets[i].setDstSet(m_mmdGroundShadowDescSet);
		device.updateDescriptorSets(writesCount, mmdGroundShadowWriteDescSets,
		                            0, nullptr);
	}
	return true;
}

auto Model::SetupCommandBuffer(const VKAppContext& appContext) -> bool {
	const auto device = appContext.m_device;
	const auto imgCount = appContext.m_imageCount;
	std::vector<vk::CommandBuffer> cmdBuffers(appContext.m_imageCount);
	const auto cmdBufInfo = vk::CommandBufferAllocateInfo()
			.setCommandBufferCount(imgCount)
			.setCommandPool(appContext.m_commandPool)
			.setLevel(vk::CommandBufferLevel::eSecondary);
	const vk::Result ret = device.allocateCommandBuffers(&cmdBufInfo, cmdBuffers.data());
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to allocate Models Command Buffer.\n";
		return false;
	}
	m_cmdBuffers = std::move(cmdBuffers);
	return true;
}

void Model::Destroy(const VKAppContext& appContext) {
	const auto device = appContext.m_device;
	m_indexBuffer.Clear(appContext);
	for (const auto& mat : m_materials) {
		device.destroySampler(mat.m_mmdTexSampler, nullptr);
		device.destroySampler(mat.m_mmdToonTexSampler, nullptr);
		device.destroySampler(mat.m_mmdSphereTexSampler, nullptr);
	}
	m_materials.clear();
	auto& modelRes = m_resource.m_modelResource;
	modelRes.m_vertexBuffer.Clear(appContext);
	modelRes.m_uniformBuffer.Clear(appContext);
	device.freeCommandBuffers(appContext.m_commandPool,
		static_cast<uint32_t>(m_cmdBuffers.size()), m_cmdBuffers.data());
	m_cmdBuffers.clear();
	device.destroyDescriptorPool(m_descPool, nullptr);
	m_mmdModel.reset();
	m_vmdAnim.reset();
}

void Model::UpdateAnimation(const VKAppContext& appContext) const {
	m_mmdModel->BeginAnimation();
	m_mmdModel->UpdateAllAnimation(m_vmdAnim.get(), appContext.m_animTime * 30.0f, appContext.m_elapsed);
}

void Model::Update(VKAppContext& appContext) {
	auto& [m_modelResource, m_materialResources] = m_resource;
	const auto device = appContext.m_device;
	const size_t vtxCount = m_mmdModel->m_positions.size();
	m_mmdModel->Update();
	const glm::vec3* position = m_mmdModel->m_updatePositions.data();
	const glm::vec3* normal = m_mmdModel->m_updateNormals.data();
	const glm::vec2* uv = m_mmdModel->m_updateUVs.data();
	const auto memSize = sizeof(VKVertex) * vtxCount;
	VKStagingBuffer* vbStBuf;
	if (!appContext.GetStagingBuffer(memSize, &vbStBuf)) {
		std::cout << "Failed to get Staging Buffer.\n";
		return;
	}
	void* vbStMem;
	vk::Result ret = device.mapMemory(vbStBuf->m_memory, 0, memSize, vk::MemoryMapFlags(), &vbStMem);
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to map memory.\n";
		return;
	}
	auto v = static_cast<VKVertex*>(vbStMem);
	for (size_t i = 0; i < vtxCount; i++) {
		v->m_position = *position;
		v->m_normal = *normal;
		v->m_uv = *uv;
		v++;
		position++;
		normal++;
		uv++;
	}
	device.unmapMemory(vbStBuf->m_memory);
	if (!vbStBuf->CopyBuffer(appContext, m_modelResource.m_vertexBuffer.m_buffer, memSize)) {
		std::cout << "Failed to copy buffer.\n";
		return;
	}
	const auto ubMemSize = m_modelResource.m_uniformBuffer.m_memorySize;
	VKStagingBuffer* ubStBuf;
	if (!appContext.GetStagingBuffer(ubMemSize, &ubStBuf)) {
		std::cout << "Failed to get Staging Buffer.\n";
		return;
	}
	uint8_t* ubPtr;
	ret = device.mapMemory(ubStBuf->m_memory, 0, ubMemSize, vk::MemoryMapFlags(), reinterpret_cast<void**>(&ubPtr));
	if (vk::Result::eSuccess != ret) {
		std::cout << "Failed to map memory.\n";
		return;
	}
	const auto& modelRes = m_modelResource;
	const auto world = glm::scale(glm::mat4(1.0f), glm::vec3(m_scale));
	const auto& view = appContext.m_viewMat;
	const auto& proj = appContext.m_projMat;
	constexpr auto vkMat = glm::mat4(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.5f, 0.0f,
		0.0f, 0.0f, 0.5f, 1.0f
	);
	const auto wv = view;
	const auto wvp = vkMat * proj * view;
	const auto mmdVSUB = reinterpret_cast<VKVertxShaderUB*>(ubPtr + m_modelResource.m_mmdVSUBOffset);
	mmdVSUB->m_wv = wv;
	mmdVSUB->m_wvp = wvp;
	const auto mmdEdgeVSUB = reinterpret_cast<VKEdgeVertexShaderUB*>(ubPtr + m_modelResource.m_mmdEdgeVSUBOffset);
	mmdEdgeVSUB->m_wv = wv;
	mmdEdgeVSUB->m_wvp = wvp;
	mmdEdgeVSUB->m_screenSize = glm::vec2(static_cast<float>(appContext.m_screenWidth), static_cast<float>(appContext.m_screenHeight));
	const auto mmdGroundShadowVSUB = reinterpret_cast<VKGroundShadowVertexShaderUB*>(
		ubPtr + m_modelResource.m_mmdGroundShadowVSUBOffset);
	constexpr auto plane = glm::vec4(0, 1, 0, 0);
	const auto light = -appContext.m_lightDir;
	auto shadow = glm::mat4(1);
	shadow[0][0] = plane.y * light.y + plane.z * light.z;
	shadow[0][1] = -plane.x * light.y;
	shadow[0][2] = -plane.x * light.z;
	shadow[0][3] = 0;
	shadow[1][0] = -plane.y * light.x;
	shadow[1][1] = plane.x * light.x + plane.z * light.z;
	shadow[1][2] = -plane.y * light.z;
	shadow[1][3] = 0;
	shadow[2][0] = -plane.z * light.x;
	shadow[2][1] = -plane.z * light.y;
	shadow[2][2] = plane.x * light.x + plane.y * light.y;
	shadow[2][3] = 0;
	shadow[3][0] = -plane.w * light.x;
	shadow[3][1] = -plane.w * light.y;
	shadow[3][2] = -plane.w * light.z;
	shadow[3][3] = plane.x * light.x + plane.y * light.y + plane.z * light.z;
	const auto wsvp = vkMat * proj * view * shadow * world;
	mmdGroundShadowVSUB->m_wvp = wsvp;
	const auto matCount = m_mmdModel->m_materials.size();
	for (size_t i = 0; i < matCount; i++) {
		const auto& mmdMat = m_mmdModel->m_materials[i];
		const auto& matRes = m_materialResources[i];
		const auto mmdFSUB = reinterpret_cast<VKFragmentShaderUB*>(ubPtr + matRes.m_mmdFSUBOffset);
		const auto& mat = m_materials[i];
		mmdFSUB->m_alpha = mmdMat.m_diffuse.a;
		mmdFSUB->m_diffuse = mmdMat.m_diffuse;
		mmdFSUB->m_ambient = mmdMat.m_ambient;
		mmdFSUB->m_specular = mmdMat.m_specular;
		mmdFSUB->m_specularPower = mmdMat.m_specularPower;
		if (!mmdMat.m_texture.empty()) {
			if (!mat.m_mmdTex->m_hasAlpha)
				mmdFSUB->m_textureModes.x = 1;
			else
				mmdFSUB->m_textureModes.x = 2;
			mmdFSUB->m_texMulFactor = mmdMat.m_textureMulFactor;
			mmdFSUB->m_texAddFactor = mmdMat.m_textureAddFactor;
		} else
			mmdFSUB->m_textureModes.x = 0;
		if (!mmdMat.m_toonTexture.empty()) {
			mmdFSUB->m_textureModes.y = 1;
			mmdFSUB->m_toonTexMulFactor = mmdMat.m_toonTextureMulFactor;
			mmdFSUB->m_toonTexAddFactor = mmdMat.m_toonTextureAddFactor;
		} else {
			mmdFSUB->m_textureModes.y = 0;
			mmdFSUB->m_toonTexMulFactor = mmdMat.m_toonTextureMulFactor;
			mmdFSUB->m_toonTexAddFactor = mmdMat.m_toonTextureAddFactor;
		}
		if (!mmdMat.m_spTexture.empty()) {
			if (mmdMat.m_spTextureMode == SphereMode::Mul)
				mmdFSUB->m_textureModes.z = 1;
			else if (mmdMat.m_spTextureMode == SphereMode::Add)
				mmdFSUB->m_textureModes.z = 2;
			mmdFSUB->m_sphereTexMulFactor = mmdMat.m_spTextureMulFactor;
			mmdFSUB->m_sphereTexAddFactor = mmdMat.m_spTextureAddFactor;
		} else {
			mmdFSUB->m_textureModes.z = 0;
			mmdFSUB->m_sphereTexMulFactor = mmdMat.m_spTextureMulFactor;
			mmdFSUB->m_sphereTexAddFactor = mmdMat.m_spTextureAddFactor;
		}
		mmdFSUB->m_lightColor = appContext.m_lightColor;
		glm::vec3 lightDir = appContext.m_lightDir;
		auto viewMat = glm::mat3(appContext.m_viewMat);
		lightDir = viewMat * lightDir;
		mmdFSUB->m_lightDir = lightDir;
		const auto mmdEdgeSizeVSUB = reinterpret_cast<VKEdgeSizeVertexShaderUB*>(ubPtr + matRes.m_mmdEdgeSizeVSUBOffset);
		mmdEdgeSizeVSUB->m_edgeSize = mmdMat.m_edgeSize;
		const auto mmdEdgeFSUB = reinterpret_cast<VKEdgeFragmentShaderUB*>(ubPtr + matRes.m_mmdEdgeFSUBOffset);
		mmdEdgeFSUB->m_edgeColor = mmdMat.m_edgeColor;
		const auto mmdGroundShadowFSUB = reinterpret_cast<VKGroundShadowFragmentShaderUB*>(
			ubPtr + matRes.m_mmdGroundShadowFSUBOffset);
		mmdGroundShadowFSUB->m_shadowColor = glm::vec4(0.4f, 0.2f, 0.2f, 0.7f);
	}
	device.unmapMemory(ubStBuf->m_memory);
	ubStBuf->CopyBuffer(appContext, modelRes.m_uniformBuffer.m_buffer, ubMemSize);
}

void Model::Draw(const VKAppContext& appContext) {
	const auto inheritanceInfo = vk::CommandBufferInheritanceInfo()
			.setRenderPass(appContext.m_renderPass)
			.setSubpass(0);
	const auto beginInfo = vk::CommandBufferBeginInfo()
			.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue)
			.setPInheritanceInfo(&inheritanceInfo);
	auto& [m_modelResource, m_materialResources] = m_resource;
	const auto& modelRes = m_modelResource;
	const auto& cmdBuf = m_cmdBuffers[appContext.m_imageIndex];
	const auto width = appContext.m_screenWidth;
	const auto height = appContext.m_screenHeight;
	cmdBuf.begin(beginInfo);
	const auto viewport = vk::Viewport()
			.setWidth(static_cast<float>(width))
			.setHeight(static_cast<float>(height))
			.setMinDepth(0.0f)
			.setMaxDepth(1.0f);
	cmdBuf.setViewport(0, 1, &viewport);
	const auto scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height));
	cmdBuf.setScissor(0, 1, &scissor);
	constexpr vk::DeviceSize offsets[1] = {};
	cmdBuf.bindVertexBuffers(0, 1, &modelRes.m_vertexBuffer.m_buffer, offsets);
	cmdBuf.bindIndexBuffer(m_indexBuffer.m_buffer, 0, m_indexType);
	const vk::Pipeline* currentMMDPipeline = nullptr;
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& matID = m_materialID;
		const auto& mmdMat = m_mmdModel->m_materials[matID];
		auto& matRes = m_materialResources[matID];
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;
		cmdBuf.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			appContext.m_mmdPipelineLayout,
			0, 1, &matRes.m_mmdDescSet,
			0, nullptr);
		const vk::Pipeline* mmdPipeline = nullptr;
		if (mmdMat.m_bothFace)
			mmdPipeline = &appContext.m_mmdPipelines[static_cast<int>(VKAppContext::MMDRenderType::AlphaBlend_BothFace)];
		else
			mmdPipeline = &appContext.m_mmdPipelines[static_cast<int>(VKAppContext::MMDRenderType::AlphaBlend_FrontFace)];
		if (currentMMDPipeline != mmdPipeline) {
			cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, *mmdPipeline);
			currentMMDPipeline = mmdPipeline;
		}
		cmdBuf.drawIndexed(m_vertexCount, 1, m_beginIndex, 0, 1);
	}
	cmdBuf.bindPipeline(
		vk::PipelineBindPoint::eGraphics,
		appContext.m_mmdEdgePipeline);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& matID = m_materialID;
		const auto& mmdMat = m_mmdModel->m_materials[matID];
		auto& matRes = m_materialResources[matID];
		if (!mmdMat.m_edgeFlag)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;
		cmdBuf.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			appContext.m_mmdEdgePipelineLayout,
			0, 1, &matRes.m_mmdEdgeDescSet,
			0, nullptr);
		cmdBuf.drawIndexed(m_vertexCount, 1, m_beginIndex, 0, 1);
	}
	cmdBuf.bindPipeline(
		vk::PipelineBindPoint::eGraphics,
		appContext.m_mmdGroundShadowPipeline);
	cmdBuf.setDepthBias(-1.0f, 0.0f, -1.0f);
	cmdBuf.setStencilReference(vk::StencilFaceFlagBits::eVkStencilFrontAndBack, 0x01);
	cmdBuf.setStencilCompareMask(vk::StencilFaceFlagBits::eVkStencilFrontAndBack, 0x01);
	cmdBuf.setStencilWriteMask(vk::StencilFaceFlagBits::eVkStencilFrontAndBack, 0xff);
	for (const auto& [m_beginIndex, m_vertexCount, m_materialID] : m_mmdModel->m_subMeshes) {
		const auto& matID = m_materialID;
		const auto& mmdMat = m_mmdModel->m_materials[matID];
		auto& matRes = m_materialResources[matID];
		if (!mmdMat.m_groundShadow)
			continue;
		if (mmdMat.m_diffuse.a == 0.0f)
			continue;
		cmdBuf.bindDescriptorSets(
			vk::PipelineBindPoint::eGraphics,
			appContext.m_mmdGroundShadowPipelineLayout,
			0, 1, &matRes.m_mmdGroundShadowDescSet,
			0, nullptr);
		cmdBuf.drawIndexed(m_vertexCount, 1, m_beginIndex, 0, 1);
	}
	cmdBuf.end();
}

vk::CommandBuffer Model::GetCommandBuffer(const uint32_t imageIndex) const {
	return m_cmdBuffers[imageIndex];
}

bool VKSampleMain(const SceneConfig& cfg) {
	MusicUtil music;
	music.Init(cfg.musicPath);
	if (!glfwInit())
		return false;
	if (!glfwVulkanSupported()) {
		std::cout << "Does not support Vulkan.\n";
		glfwTerminate();
		return false;
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Pmx Mod (Vulkan)", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return false;
	}
	std::vector<const char*> extensions;
	uint32_t extCount = 0;
	const char** glfwExt = glfwGetRequiredInstanceExtensions(&extCount);
	extensions.reserve(extCount);
	for (uint32_t i = 0; i < extCount; ++i)
		extensions.push_back(glfwExt[i]);
	std::vector<const char*> layers;
	vk::Instance instance;
	vk::Result ret;
	auto instInfo = vk::InstanceCreateInfo()
			.setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()))
			.setPpEnabledExtensionNames(extensions.data())
			.setEnabledLayerCount(static_cast<uint32_t>(layers.size()))
			.setPpEnabledLayerNames(layers.data());
	ret = vk::createInstance(&instInfo, nullptr, &instance);
	if (ret != vk::Result::eSuccess) {
		std::cout << "Failed to create Vulkan instance.\n";
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	vk::SurfaceKHR surface;
	VkSurfaceKHR rawSurface = VK_NULL_HANDLE;
	VkResult err = glfwCreateWindowSurface(instance, window, nullptr, &rawSurface);
	if (err != VK_SUCCESS) {
		std::cout << "Failed to create surface. [" << err << "]\n";
		instance.destroy();
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	surface = rawSurface;
	auto gpus = instance.enumeratePhysicalDevices();
	if (gpus.empty()) {
		std::cout << "Failed to find Vulkan physical device.\n";
		instance.destroySurfaceKHR(surface);
		instance.destroy();
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	vk::PhysicalDevice gpu = gpus[0];\
	uint32_t graphicsQ = UINT32_MAX;
	uint32_t presentQ = UINT32_MAX;
	auto qFamilies = gpu.getQueueFamilyProperties();
	for (uint32_t i = 0; i < static_cast<uint32_t>(qFamilies.size()); ++i) {
		const bool hasGraphics = (qFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
		const bool hasPresent = gpu.getSurfaceSupportKHR(i, surface);
		if (hasGraphics && graphicsQ == UINT32_MAX) graphicsQ = i;
		if (hasPresent && presentQ == UINT32_MAX) presentQ = i;
		if (hasGraphics && hasPresent) {
			graphicsQ = i;
			presentQ = i;
			break;
		}
	}
	if (graphicsQ == UINT32_MAX || presentQ == UINT32_MAX) {
		std::cout << "Failed to find required queue families.\n";
		instance.destroySurfaceKHR(surface);
		instance.destroy();
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	std::vector deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	float priority = 1.0f;
	vk::DeviceQueueCreateInfo queueInfo =
			vk::DeviceQueueCreateInfo().setQueueFamilyIndex(graphicsQ).setQueueCount(1).setPQueuePriorities(&priority);
	vk::PhysicalDeviceFeatures features;
	features.setSampleRateShading(true);
	vk::Device device;
	vk::DeviceCreateInfo devInfo = vk::DeviceCreateInfo()
			.setQueueCreateInfoCount(1)
			.setPQueueCreateInfos(&queueInfo)
			.setEnabledExtensionCount(static_cast<uint32_t>(deviceExtensions.size()))
			.setPpEnabledExtensionNames(deviceExtensions.data())
			.setPEnabledFeatures(&features);
	ret = gpu.createDevice(&devInfo, nullptr, &device);
	if (ret != vk::Result::eSuccess) {
		std::cout << "Failed to create device.\n";
		instance.destroySurfaceKHR(surface);
		instance.destroy();
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	VKAppContext appContext;
	int w1, h1;
	glfwGetFramebufferSize(window, &w1, &h1);
	appContext.m_screenWidth = w1;
	appContext.m_screenHeight = h1;
	if (!appContext.Setup(instance, surface, gpu, device)) {
		std::cout << "Failed to setup Vulkan AppContext.\n";
		device.destroy();
		instance.destroySurfaceKHR(surface);
		instance.destroy();
		glfwDestroyWindow(window);
		glfwTerminate();
		return false;
	}
	if (!cfg.cameraVmd.empty()) {
		VMDReader camVmd;
		if (camVmd.ReadVMDFile(cfg.cameraVmd.c_str()) && !camVmd.m_cameras.empty()) {
			auto vmdCamAnim = std::make_unique<VMDCameraAnimation>();
			if (!vmdCamAnim->Create(camVmd))
				std::cout << "Failed to create VMDCameraAnimation.\n";
			appContext.m_vmdCameraAnim = std::move(vmdCamAnim);
		}
	}
	std::vector<Model> models;
	models.reserve(cfg.models.size());
	for (const auto& [m_modelPath, m_vmdPaths, m_scale] : cfg.models) {
		Model model;
		const auto ext = PathUtil::GetExt(m_modelPath);
		if (ext != "pmx") {
			std::cout << "Unknown file type. [" << ext << "]\n";
			device.waitIdle();
			for (auto& m : models)
				m.Destroy(appContext);
			appContext.Destroy();
			device.destroy();
			instance.destroySurfaceKHR(surface);
			instance.destroy();
			glfwDestroyWindow(window);
			glfwTerminate();
			return false;
		}
		auto pmxModel = std::make_unique<MMDModel>();
		if (!pmxModel->Load(m_modelPath, appContext.m_mmdDir)) {
			std::cout << "Failed to load pmx file.\n";
			device.waitIdle();
			for (auto& m : models)
				m.Destroy(appContext);
			appContext.Destroy();
			device.destroy();
			instance.destroySurfaceKHR(surface);
			instance.destroy();
			glfwDestroyWindow(window);
			glfwTerminate();
			return false;
		}
		model.m_mmdModel = std::move(pmxModel);
		model.m_mmdModel->InitializeAnimation();
		auto vmdAnim = std::make_unique<VMDAnimation>();
		vmdAnim->m_model = model.m_mmdModel;
		for (const auto& vmdPath : m_vmdPaths) {
			VMDReader vmd;
			if (!vmd.ReadVMDFile(vmdPath.c_str())) {
				std::cout << "Failed to read VMD file.\n";
				device.waitIdle();
				for (auto& m : models)
					m.Destroy(appContext);
				appContext.Destroy();
				device.destroy();
				instance.destroySurfaceKHR(surface);
				instance.destroy();
				glfwDestroyWindow(window);
				glfwTerminate();
				return false;
			}
			if (!vmdAnim->Add(vmd)) {
				std::cout << "Failed to add VMDAnimation.\n";
				device.waitIdle();
				for (auto& m : models)
					m.Destroy(appContext);
				appContext.Destroy();
				device.destroy();
				instance.destroySurfaceKHR(surface);
				instance.destroy();
				glfwDestroyWindow(window);
				glfwTerminate();
				return false;
			}
		}
		vmdAnim->SyncPhysics(0.0f);
		model.m_vmdAnim = std::move(vmdAnim);
		model.m_scale = m_scale;
		model.Setup(appContext);
		models.emplace_back(std::move(model));
	}
	auto fpsTime = std::chrono::steady_clock::now();
	auto saveTime = std::chrono::steady_clock::now();
	int fpsFrame = 0;
	std::vector<vk::Semaphore> waitSemaphores;
	std::vector<vk::PipelineStageFlags> waitStages;
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		auto now = std::chrono::steady_clock::now();
		double elapsed = std::chrono::duration<double>(now - saveTime).count();
		if (elapsed > 1.0 / 30.0) elapsed = 1.0 / 30.0;
		saveTime = now;
		auto dt = static_cast<float>(elapsed);
		float t = appContext.m_animTime + dt;
		if (music.HasMusic()) {
			auto [adt, at] = music.PullTimes();
			if (adt < 0.f) adt = 0.f;
			dt = adt;
			t = at;
		}
		appContext.m_elapsed = dt;
		appContext.m_animTime = t;
		const uint32_t frameIndex = appContext.m_frameIndex % static_cast<uint32_t>(appContext.m_frameSyncDatas.size());
		auto& [m_fence, m_presentCompleteSemaphore, m_renderCompleteSemaphore] = appContext.m_frameSyncDatas[frameIndex];
		if (appContext.m_device.waitForFences(1, &m_fence, VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
			std::cout << "Failed to wait fence.\n";
			break;
		}
		if (appContext.m_device.resetFences(1, &m_fence) != vk::Result::eSuccess) {
			std::cout << "Failed to reset fence.\n";
			break;
		}
		vk::Result ret2 = appContext.m_device.acquireNextImageKHR(
			appContext.m_swapChain,
			UINT64_MAX,
			m_presentCompleteSemaphore,
			vk::Fence(),
			&appContext.m_imageIndex
		);
		if (ret2 == vk::Result::eErrorOutOfDateKHR) {
			appContext.Resize();
			return false;
		}
		if (ret2 != vk::Result::eSuccess && ret2 != vk::Result::eSuboptimalKHR) {
			std::cout << "acquireNextImageKHR failed\n";
			return false;
		}
		int w2, h2;
		glfwGetFramebufferSize(window, &w2, &h2);
		if (w2 != appContext.m_screenWidth || h2 != appContext.m_screenHeight) {
			appContext.m_screenWidth = w2;
			appContext.m_screenHeight = h2;
			if (!appContext.Resize()) {
				std::cout << "Resize failed.\n";
				break;
			}
		}
		const int width = appContext.m_screenWidth;
		const int height = appContext.m_screenHeight;
		if (appContext.m_vmdCameraAnim) {
			appContext.m_vmdCameraAnim->Evaluate(appContext.m_animTime * 30.0f);
			const auto mmdCam = appContext.m_vmdCameraAnim->m_camera;
			appContext.m_viewMat = mmdCam.GetViewMatrix();
			appContext.m_projMat = glm::perspectiveFovRH(
				mmdCam.m_fov,
				static_cast<float>(width),
				static_cast<float>(height),
				1.0f, 10000.0f
			);
		} else {
			appContext.m_viewMat = glm::lookAt(glm::vec3(0, 10, 40), glm::vec3(0, 10, 0), glm::vec3(0, 1, 0));
			appContext.m_projMat = glm::perspectiveFovRH(glm::radians(30.0f),
				static_cast<float>(width), static_cast<float>(height), 1.0f, 10000.0f);
		}
		uint32_t imgIndex = appContext.m_imageIndex;
		auto& res = appContext.m_swapChainImageResources[imgIndex];
		vk::CommandBuffer cmdBuffer = res.m_cmdBuffer;
		for (auto& model : models) {
			model.UpdateAnimation(appContext);
			model.Update(appContext);
		}
		for (auto& model : models)
			model.Draw(appContext);
		cmdBuffer.begin(vk::CommandBufferBeginInfo());
		auto clearColor = vk::ClearColorValue(std::array<float, 4>({ 1.0f, 0.8f, 0.75f, 1.0f }));
		auto clearDepth = vk::ClearDepthStencilValue(1.0f, 0);
		vk::ClearValue clearValues[] = {
			clearColor,
			clearColor,
			clearDepth,
			clearDepth,
		};
		vk::RenderPassBeginInfo rpBegin = vk::RenderPassBeginInfo()
			.setRenderPass(appContext.m_renderPass)
			.setFramebuffer(res.m_framebuffer)
			.setRenderArea(vk::Rect2D({ 0,0 },
				{ static_cast<uint32_t>(appContext.m_screenWidth),
					static_cast<uint32_t>(appContext.m_screenHeight) }))
			.setClearValueCount(std::size(clearValues))
			.setPClearValues(clearValues);
		cmdBuffer.beginRenderPass(rpBegin, vk::SubpassContents::eSecondaryCommandBuffers);
		for (auto& model : models) {
			vk::CommandBuffer sec = model.GetCommandBuffer(imgIndex);
			cmdBuffer.executeCommands(1, &sec);
		}
		cmdBuffer.endRenderPass();
		cmdBuffer.end();
		waitSemaphores.push_back(m_presentCompleteSemaphore);
		waitStages.emplace_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		for (const auto& stBuf : appContext.m_stagingBuffers) {
			if (stBuf->m_waitSemaphore) {
				waitSemaphores.push_back(stBuf->m_waitSemaphore);
				waitStages.emplace_back(vk::PipelineStageFlagBits::eTransfer);
				stBuf->m_waitSemaphore = nullptr;
			}
		}
		vk::SubmitInfo submit = vk::SubmitInfo()
			.setPWaitDstStageMask(waitStages.data())
			.setWaitSemaphoreCount(static_cast<uint32_t>(waitSemaphores.size()))
			.setPWaitSemaphores(waitSemaphores.data())
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&m_renderCompleteSemaphore)
			.setCommandBufferCount(1)
			.setPCommandBuffers(&cmdBuffer);
		if (appContext.m_graphicsQueue.submit(1, &submit, m_fence) != vk::Result::eSuccess) {
			std::cout << "Failed to submit to graphics queue.\n";
			break;
		}
		waitSemaphores.clear();
		waitStages.clear();
		vk::PresentInfoKHR present = vk::PresentInfoKHR()
			.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&m_renderCompleteSemaphore)
			.setSwapchainCount(1)
			.setPSwapchains(&appContext.m_swapChain)
			.setPImageIndices(&imgIndex);
		ret2 = appContext.m_graphicsQueue.presentKHR(present);
		if (ret2 == vk::Result::eErrorOutOfDateKHR || ret2 == vk::Result::eSuboptimalKHR) {
			appContext.Resize();
		} else if (ret2 != vk::Result::eSuccess) {
			std::cout << "presentKHR failed\n";
			return false;
		}
		appContext.m_frameIndex = (appContext.m_frameIndex + 1) % static_cast<uint32_t>(appContext.m_frameSyncDatas.size());
		fpsFrame++;
		double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - fpsTime).count();
		if (sec > 1.0) {
			std::cout << (fpsFrame / sec) << " fps\n";
			fpsFrame = 0;
			fpsTime = std::chrono::steady_clock::now();
		}
	}
	device.waitIdle();
	for (auto& model : models)
		model.Destroy(appContext);
	models.clear();
	appContext.Destroy();
	device.destroy();
	instance.destroySurfaceKHR(surface);
	instance.destroy();
	glfwDestroyWindow(window);
	glfwTerminate();
	return true;
}
