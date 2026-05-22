#pragma once

#include "VulkanDevice.h"

#include <array>

namespace Chrivent {
	struct VulkanSyncObjectInfo {
		static constexpr size_t kMaxFramesInFlight = 2;
		std::array<VkSemaphore, kMaxFramesInFlight> imageAvailableSemaphores{};
		std::array<VkSemaphore, kMaxFramesInFlight> renderFinishedSemaphores{};
		std::array<VkFence, kMaxFramesInFlight> inFlightFences{};
		size_t currentFrame = 0;
	};

	class VulkanSyncObject {
		VulkanSyncObjectInfo info;
		VkDevice device = VK_NULL_HANDLE;

	public:
		VulkanSyncObject() = default;
		~VulkanSyncObject();

		VulkanSyncObject(const VulkanSyncObject&) = delete;
		VulkanSyncObject& operator=(const VulkanSyncObject&) = delete;
		VulkanSyncObject(VulkanSyncObject&&) = delete;
		VulkanSyncObject& operator=(VulkanSyncObject&&) = delete;
		
		VulkanSyncObjectInfo& GetInfo() { return info; }
		const VulkanSyncObjectInfo& GetInfo() const { return info; }

		// 더블버퍼링에 사용할 세마포어와 펜스를 생성한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo);
		// 생성한 세마포어와 펜스를 해제한다.
		void Destroy();
		// 다음 프레임의 동기화 객체를 사용하도록 인덱스를 넘긴다.
		void AdvanceFrame();
	};
}
