#pragma once

#include "Viewer/Buffer/DynamicBufferRing.h"
#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Synchronization/FrameBuffering.h"

#include <optional>
#include <string>

namespace Chrivent {
	// Vulkan 동적 uniform 버퍼의 프레임별 할당과 동기화를 관리한다.
	class VulkanDynamicBufferRing : public DynamicBufferRing {
		VulkanBuffer buffer;

	public:
		const VulkanBuffer& GetBuffer() const { return buffer; }

		// Vulkan 업로드 링 버퍼를 생성한다.
		GraphicsResult<void> Setup(const VulkanDevice& sourceDevice, size_t bufferSize);
		// Vulkan 업로드 링 버퍼 리소스와 공통 상태를 정리한다.
		void Clear();
		// 프레임별 링 버퍼 위치를 새 프레임에 맞춰 초기화한다.
		void BeginFrame(const size_t frameIndex) {
			DynamicBufferRing::BeginFrame(frameIndex % FrameBuffering::vulkanFramesInFlight);
		}
		// Vulkan uniform buffer offset 정렬 조건에 맞는 업로드 구간을 예약한다.
		std::optional<UploadSlice> Allocate(size_t size, size_t alignment, std::string& outError);
		// 예약한 업로드 구간에 지정한 크기의 데이터를 복사한다.
		bool Write(const UploadSlice& slice, const void* data, size_t dataSize, std::string& outError) const;
	};
}
