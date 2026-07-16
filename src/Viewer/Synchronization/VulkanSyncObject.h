#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Synchronization/FrameBuffering.h"

#include <vector>

namespace Chrivent {
	// Vulkan 프레임 진행에 필요한 세마포어와 펜스의 수명을 관리한다.
	class VulkanSyncObject {
		VkDevice device = VK_NULL_HANDLE;
		VkSemaphore imageAvailableSemaphores[FrameBuffering::vulkanFramesInFlight]{};
		VkFence inFlightFences[FrameBuffering::vulkanFramesInFlight]{};
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> imagesInFlight;
		size_t currentFrame = 0;

		// swapchain 이미지별 present 완료 세마포어를 생성한다.
		bool CreateRenderFinishedSemaphores(size_t swapChainImageCount);
		// swapchain 이미지별 present 완료 세마포어를 해제한다.
		void ResetRenderFinishedSemaphores();
		// 제출 실패 뒤 현재 프레임 fence를 신호 상태의 새 객체로 교체한다.
		bool RestoreCurrentFence();

	public:
		VulkanSyncObject() = default;
		~VulkanSyncObject();

		VulkanSyncObject(const VulkanSyncObject&) = delete;
		VulkanSyncObject& operator=(const VulkanSyncObject&) = delete;
		
		// 더블버퍼링에 사용할 세마포어와 펜스를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, size_t swapChainImageCount);
		// 생성한 세마포어와 펜스를 해제한다.
		void Reset();
		// 스왑체인 이미지별 fence 추적과 present 완료 세마포어를 초기화한다.
		bool ResetImageTracking(size_t swapChainImageCount);
		// 현재 프레임 슬롯의 이전 제출이 끝날 때까지 기다린다.
		bool WaitForCurrentFrame() const;
		// 지정한 스왑체인 이미지를 사용한 이전 제출이 끝날 때까지 기다린다.
		bool WaitForImage(uint32_t imageIndex) const;
		// 현재 command buffer를 제출하고 이미지별 fence 추적을 갱신한다.
		bool Submit(VkQueue graphicsQueue, VkCommandBuffer commandBuffer, uint32_t imageIndex);
		// 현재 프레임의 이미지 획득 세마포어를 반환한다.
		VkSemaphore GetImageAvailableSemaphore() const { return imageAvailableSemaphores[currentFrame]; }
		// 지정한 스왑체인 이미지의 렌더링 완료 세마포어를 반환한다.
		VkSemaphore GetRenderFinishedSemaphore(uint32_t imageIndex) const;
		// 현재 사용 중인 프레임 인덱스를 반환한다.
		size_t GetCurrentFrameIndex() const { return currentFrame; }
		// 다음 프레임의 동기화 객체를 사용하도록 인덱스를 넘긴다.
		void AdvanceFrame() { currentFrame = (currentFrame + 1) % FrameBuffering::vulkanFramesInFlight; }
	};
}
