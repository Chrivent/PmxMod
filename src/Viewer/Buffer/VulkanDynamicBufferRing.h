#pragma once

#include "Viewer/Buffer/DynamicBufferRing.h"
#include "Viewer/Buffer/VulkanBuffer.h"

#include <optional>
#include <string>

namespace Chrivent {
	class VulkanDynamicBufferRing : public DynamicBufferRing {
		static constexpr size_t kBufferedFrames = 2;
		VulkanBuffer buffer;

	public:
		~VulkanDynamicBufferRing() override = default;
		
		const VulkanBuffer& GetBuffer() const { return buffer; }

		// Vulkan 업로드 링 버퍼를 생성한다.
		bool Setup(const VulkanDevice& sourceDevice, size_t bufferSize, std::string& outError);
		// Vulkan 업로드 링 버퍼 리소스와 공통 상태를 정리한다.
		void Clear() override;
		// 프레임별 링 버퍼 위치를 새 프레임에 맞춰 초기화한다.
		void BeginFrame(const size_t frameIndex) override { DynamicBufferRing::BeginFrame(frameIndex % kBufferedFrames); }
		// Vulkan uniform buffer offset 정렬 조건에 맞는 업로드 구간을 예약한다.
		std::optional<UploadSlice> Allocate(size_t size, size_t alignment, std::string& outError) override;
		// 예약한 업로드 구간에 데이터를 복사한다.
		bool Write(const UploadSlice& slice, const void* data, std::string& outError) const;
	};
}
