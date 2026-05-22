#pragma once

#include "VulkanDevice.h"

#include <array>

namespace Chrivent {
	class VulkanSyncObject {
		static constexpr size_t kMaxFramesInFlight = 2;

		std::array<VkSemaphore, kMaxFramesInFlight> imageAvailableSemaphores{};
		std::array<VkSemaphore, kMaxFramesInFlight> renderFinishedSemaphores{};
		std::array<VkFence, kMaxFramesInFlight> inFlightFences{};
		VkDevice device = VK_NULL_HANDLE;
		size_t currentFrame = 0;

	public:
		VulkanSyncObject() = default;
		~VulkanSyncObject();

		VulkanSyncObject(const VulkanSyncObject&) = delete;
		VulkanSyncObject& operator=(const VulkanSyncObject&) = delete;
		VulkanSyncObject(VulkanSyncObject&&) = delete;
		VulkanSyncObject& operator=(VulkanSyncObject&&) = delete;

		// ?붾툝踰꾪띁留곸뿉 ?ъ슜???몃쭏?ъ뼱? ?쒖뒪瑜??앹꽦?쒕떎.
		bool Initialize(const VulkanDeviceInfo& deviceInfo);
		// ?앹꽦???몃쭏?ъ뼱? ?쒖뒪瑜??댁젣?쒕떎.
		void Destroy();
		// ?ㅼ쓬 ?꾨젅?꾩쓽 ?숆린??媛앹껜瑜??ъ슜?섎룄濡??몃뜳?ㅻ? ?섍릿??
		void AdvanceFrame();
	};
}
