#include "Viewer/Synchronization/VulkanSyncObject.h"

namespace Chrivent {
	GraphicsResult<void> VulkanSyncObject::CreateRenderFinishedSemaphores(
		const size_t swapChainImageCount) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		renderFinishedSemaphores.assign(swapChainImageCount, VK_NULL_HANDLE);
		for (auto& semaphore : renderFinishedSemaphores) {
			const VkResult result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore);
			if (result != VK_SUCCESS) {
				ResetRenderFinishedSemaphores();
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "present semaphore 생성",
					"Vulkan present semaphore를 만들지 못했습니다", result, true));
			}
		}
		return {};
	}

	void VulkanSyncObject::ResetRenderFinishedSemaphores() {
		if (device != VK_NULL_HANDLE) {
			for (const VkSemaphore semaphore : renderFinishedSemaphores) {
				if (semaphore != VK_NULL_HANDLE)
					vkDestroySemaphore(device, semaphore, nullptr);
			}
		}
		renderFinishedSemaphores.clear();
	}

	GraphicsResult<void> VulkanSyncObject::RestoreCurrentFence() {
		VkFence& currentFence = inFlightFences[currentFrame];
		const VkFence discardedFence = currentFence;
		for (VkFence& imageFence : imagesInFlight) {
			if (imageFence == discardedFence)
				imageFence = VK_NULL_HANDLE;
		}
		if (discardedFence != VK_NULL_HANDLE)
			vkDestroyFence(device, discardedFence, nullptr);
		currentFence = VK_NULL_HANDLE;
		constexpr VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT
		};
		const VkResult result = vkCreateFence(device, &fenceInfo, nullptr, &currentFence);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "in-flight fence 복구",
				"제출 실패 후 Vulkan in-flight fence를 복구하지 못했습니다", result, true));
		}
		return {};
	}

	VulkanSyncObject::~VulkanSyncObject() {
		Reset();
	}

	GraphicsResult<void> VulkanSyncObject::Initialize(
		const VulkanDevice& sourceDevice, const size_t swapChainImageCount) {
		Reset();
		device = sourceDevice.GetDevice();
		if (device == VK_NULL_HANDLE || swapChainImageCount == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "프레임 동기화 초기화",
				"Vulkan device 또는 스왑체인 이미지 수가 올바르지 않습니다"));
		}
		currentFrame = 0;
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		for (size_t i = 0; i < FrameBuffering::vulkanFramesInFlight; i++) {
			VkResult result = vkCreateSemaphore(
				device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
			if (result == VK_SUCCESS)
				result = vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]);
			if (result != VK_SUCCESS) {
				Reset();
				return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
					GraphicsErrorCode::ResourceCreationFailed, "프레임 동기화 객체 생성",
					"Vulkan 프레임 세마포어 또는 fence를 만들지 못했습니다", result, true));
			}
		}
		const auto trackingResult = ResetImageTracking(swapChainImageCount);
		if (trackingResult)
			return {};
		const GraphicsError error = trackingResult.error();
		Reset();
		return std::unexpected(error);
	}

	void VulkanSyncObject::Reset() {
		ResetRenderFinishedSemaphores();
		for (size_t i = 0; i < FrameBuffering::vulkanFramesInFlight; i++) {
			if (device != VK_NULL_HANDLE && imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			}
			if (device != VK_NULL_HANDLE && inFlightFences[i] != VK_NULL_HANDLE) {
				vkDestroyFence(device, inFlightFences[i], nullptr);
			}
			imageAvailableSemaphores[i] = VK_NULL_HANDLE;
			inFlightFences[i] = VK_NULL_HANDLE;
		}
		imagesInFlight.clear();
		currentFrame = 0;
		device = VK_NULL_HANDLE;
	}

	GraphicsResult<void> VulkanSyncObject::ResetImageTracking(const size_t swapChainImageCount) {
		if (device == VK_NULL_HANDLE || swapChainImageCount == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "스왑체인 이미지 동기화 초기화",
				"Vulkan device 또는 스왑체인 이미지 수가 올바르지 않습니다"));
		}
		ResetRenderFinishedSemaphores();
		imagesInFlight.assign(swapChainImageCount, VK_NULL_HANDLE);
		const auto result = CreateRenderFinishedSemaphores(swapChainImageCount);
		if (!result)
			imagesInFlight.clear();
		return result;
	}

	GraphicsResult<void> VulkanSyncObject::WaitForCurrentFrame() const {
		if (device == VK_NULL_HANDLE || inFlightFences[currentFrame] == VK_NULL_HANDLE) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "현재 프레임 대기",
				"Vulkan 현재 프레임 fence를 사용할 수 없습니다"));
		}
		const VkResult result = vkWaitForFences(
			device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::SynchronizationFailed, "현재 프레임 대기",
				"Vulkan 프레임 fence를 기다리지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> VulkanSyncObject::WaitForImage(const uint32_t imageIndex) const {
		if (device == VK_NULL_HANDLE || imageIndex >= imagesInFlight.size()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "스왑체인 이미지 대기",
				"Vulkan 스왑체인 이미지 색인이 동기화 범위를 벗어났습니다"));
		}
		const VkFence imageFence = imagesInFlight[imageIndex];
		if (imageFence == VK_NULL_HANDLE)
			return {};
		const VkResult result = vkWaitForFences(device, 1, &imageFence, VK_TRUE, UINT64_MAX);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::SynchronizationFailed, "스왑체인 이미지 대기",
				"Vulkan 이미지 fence를 기다리지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> VulkanSyncObject::Submit(
		const VkQueue graphicsQueue, const VkCommandBuffer commandBuffer, const uint32_t imageIndex) {
		if (device == VK_NULL_HANDLE || graphicsQueue == VK_NULL_HANDLE
			|| commandBuffer == VK_NULL_HANDLE || imageIndex >= renderFinishedSemaphores.size()
			|| imageIndex >= imagesInFlight.size()) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "프레임 제출",
				"Vulkan 프레임 제출에 필요한 큐, command buffer 또는 이미지 색인이 올바르지 않습니다"));
		}
		const VkSemaphoreSubmitInfo waitSemaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = imageAvailableSemaphores[currentFrame],
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		const VkSemaphoreSubmitInfo signalSemaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = renderFinishedSemaphores[imageIndex],
			.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
		};
		const VkCommandBufferSubmitInfo commandBufferInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = commandBuffer
		};
		const VkFence& inFlightFence = inFlightFences[currentFrame];
		VkResult result = vkResetFences(device, 1, &inFlightFence);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::SynchronizationFailed, "프레임 fence 초기화",
				"Vulkan in-flight fence를 초기화하지 못했습니다", result, true));
		}
		const VkSubmitInfo2 submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &waitSemaphoreInfo,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &commandBufferInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos = &signalSemaphoreInfo
		};
		result = vkQueueSubmit2(graphicsQueue, 1, &submitInfo, inFlightFence);
		if (result != VK_SUCCESS) {
			const auto restoreResult = RestoreCurrentFence();
			if (!restoreResult)
				return std::unexpected(restoreResult.error());
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandSubmissionFailed, "프레임 제출",
				"Vulkan command buffer를 제출하지 못했습니다", result, true));
		}
		imagesInFlight[imageIndex] = inFlightFence;
		return {};
	}

	VkSemaphore VulkanSyncObject::GetRenderFinishedSemaphore(const uint32_t imageIndex) const {
		return imageIndex < renderFinishedSemaphores.size()
			? renderFinishedSemaphores[imageIndex] : VK_NULL_HANDLE;
	}
}
