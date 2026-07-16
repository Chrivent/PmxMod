#pragma once

#include <cstdint>

namespace Chrivent::FrameBuffering {
	// D3D12 스왑체인과 프레임별 GPU 리소스가 공유하는 버퍼 수다.
	inline constexpr std::uint32_t dx12BufferCount = 2;
	// Vulkan CPU/GPU 동기화와 프레임별 GPU 리소스가 공유하는 동시 프레임 수다.
	inline constexpr std::size_t vulkanFramesInFlight = 2;
}
