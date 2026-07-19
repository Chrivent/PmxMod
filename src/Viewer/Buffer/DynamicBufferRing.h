#pragma once

#include <expected>
#include <string>

namespace Chrivent {
	// 동적 업로드 버퍼에서 할당된 바이트 범위를 나타낸다.
	struct UploadSlice {
		size_t offset = 0;
		size_t size = 0;
		void* cpuAddress = nullptr;
	};

	// API 독립 동적 버퍼 범위 검증에 실패한 원인을 보관한다.
	struct DynamicBufferError {
		std::string message;

		// 동적 버퍼 작업의 성공값 또는 현재 오류를 반환하는 형식이다.
		template <typename T>
		using Result = std::expected<T, DynamicBufferError>;
	};
	
	// 프레임 중 순차적으로 사용할 정렬된 동적 버퍼 범위를 할당한다.
	class DynamicBufferRing {
	protected:
		size_t capacity = 0;
		size_t writeOffset = 0;
		size_t currentFrameIndex = 0;

		// 공통 범위 검사와 write offset 갱신을 거쳐 업로드 구간을 예약한다.
		DynamicBufferError::Result<UploadSlice> AllocateSlice(size_t size, size_t alignment,
			size_t availableCapacity, size_t baseOffset, std::string capacityError);

	public:
		// 업로드 링 버퍼의 공통 상태를 초기화한다.
		DynamicBufferError::Result<void> Setup(size_t bufferSize);
		// 업로드 링 버퍼의 공통 상태를 정리한다.
		void Clear();
		// 새 프레임 시작 시 쓰기 포인터를 초기화한다.
		void BeginFrame(size_t frameIndex);
	};
}
