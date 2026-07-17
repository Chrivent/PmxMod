#pragma once

#include <cstdint>

namespace Chrivent {
	// 프레임별 GPU 리소스와 CPU/GPU 동기화가 공유하는 버퍼 수를 정의한다.
	class FrameBuffering {
	public:
		// D3D12 스왑체인과 프레임별 GPU 리소스가 공유하는 버퍼 수다.
		static constexpr std::uint32_t dx12BufferCount = 2;
		// Vulkan CPU/GPU 동기화와 프레임별 GPU 리소스가 공유하는 동시 프레임 수다.
		static constexpr std::size_t vulkanFramesInFlight = 2;
	};
}
