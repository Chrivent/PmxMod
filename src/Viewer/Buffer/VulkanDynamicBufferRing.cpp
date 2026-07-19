#include "Viewer/Buffer/VulkanDynamicBufferRing.h"

#include "Viewer/Buffer/BufferSize.h"

#include <utility>

namespace Chrivent {
	GraphicsResult<void> VulkanDynamicBufferRing::Setup(
		const VulkanDevice& sourceDevice, const size_t bufferSize) {
		Clear();
		std::string error;
		if (!DynamicBufferRing::Setup(bufferSize, error)) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "동적 uniform buffer ring 생성",
				std::move(error)));
		}
		const auto bufferResult = buffer.Initialize(sourceDevice, bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (!bufferResult) {
			DynamicBufferRing::Clear();
			return std::unexpected(bufferResult.error());
		}
		return {};
	}

	void VulkanDynamicBufferRing::Clear() {
		buffer.Reset();
		DynamicBufferRing::Clear();
	}

	std::optional<UploadSlice> VulkanDynamicBufferRing::Allocate(const size_t size, const size_t alignment, std::string& outError) {
		const size_t frameCapacity = capacity / FrameBuffering::vulkanFramesInFlight;
		if (frameCapacity == 0) {
			outError = "Vulkan 동적 버퍼 링의 프레임 용량이 0입니다.";
			return std::nullopt;
		}
		size_t frameBaseOffset = 0;
		if (!BufferSize::TryMultiply(currentFrameIndex, frameCapacity, frameBaseOffset)) {
			outError = "Vulkan 동적 버퍼 링의 프레임 시작 위치가 크기 한도를 넘습니다.";
			return std::nullopt;
		}
		return AllocateSlice(size, alignment, frameCapacity, frameBaseOffset,
			"현재 프레임에서 Vulkan 동적 버퍼 링의 남은 공간이 부족합니다.", outError);
	}

	bool VulkanDynamicBufferRing::Write(const UploadSlice& slice, const void* data,
		const size_t dataSize, std::string& outError) const {
		if (data == nullptr) {
			outError = "Vulkan dynamic buffer ring에 쓸 원본 데이터가 null입니다.";
			return false;
		}
		if (dataSize > slice.size) {
			outError = "Vulkan dynamic buffer ring 원본 데이터가 예약된 slice보다 큽니다.";
			return false;
		}
		if (!buffer.Write(data, dataSize, slice.offset)) {
			outError = "Vulkan dynamic buffer ring slice에 쓰지 못했습니다.";
			return false;
		}
		outError.clear();
		return true;
	}
}
