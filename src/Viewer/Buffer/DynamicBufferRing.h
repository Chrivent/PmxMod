#pragma once

#include <optional>
#include <string>

namespace Chrivent {
	struct UploadSlice {
		size_t offset = 0;
		size_t size = 0;
		void* cpuAddress = nullptr;
	};
	
	class DynamicBufferRing {
	protected:
		size_t capacity = 0;
		size_t writeOffset = 0;
		size_t currentFrameIndex = 0;

		// 공통 범위 검사와 write offset 갱신을 거쳐 업로드 구간을 예약한다.
		std::optional<UploadSlice> AllocateSlice(size_t size, size_t alignment, size_t availableCapacity,
			size_t baseOffset, const char* capacityError, std::string& outError);

	public:
		virtual ~DynamicBufferRing() = default;

		// 지정한 정렬 단위에 맞춰 크기나 오프셋을 올림한다.
		static size_t AlignUp(size_t value, size_t alignment);
		// 업로드 링 버퍼의 공통 상태를 초기화한다.
		virtual bool Setup(size_t bufferSize, std::string& outError);
		// 업로드 링 버퍼의 공통 상태를 정리한다.
		virtual void Clear();
		// 새 프레임 시작 시 쓰기 포인터를 초기화한다.
		virtual void BeginFrame(size_t frameIndex);
		// 지정한 크기와 정렬 조건에 맞는 업로드 구간을 예약한다.
		virtual std::optional<UploadSlice> Allocate(size_t size, size_t alignment, std::string& outError) = 0;
	};
}
