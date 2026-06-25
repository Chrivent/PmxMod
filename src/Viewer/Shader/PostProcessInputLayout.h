#pragma once

#include <cstdint>

namespace Chrivent {
	struct PostProcessInputLayout {
		// 포스트 프로세스 공통 색상 입력이다. HLSL Texture2D SceneColor : register(t0)에 대응한다.
		static constexpr uint32_t SceneColorRegister = 0;
		// 포스트 프로세스 공통 깊이 입력이다. HLSL Texture2D SceneDepth : register(t1)에 대응한다.
		static constexpr uint32_t SceneDepthRegister = 1;
		// 포스트 프로세스 공통 clamp linear sampler다. HLSL SamplerState LinearClamp : register(s0)에 대응한다.
		static constexpr uint32_t LinearClampSamplerRegister = 0;
		// 모든 API가 최종적으로 노출해야 하는 포스트 프로세스 SRV 개수다.
		static constexpr uint32_t RequiredTextureCount = 2;
	};
}
