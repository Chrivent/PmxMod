#pragma once

#include <cstdint>

namespace Chrivent {
	// 모든 렌더링 API가 공유하는 후처리 HLSL 입력 슬롯 계약을 정의한다.
	struct PostProcessInputLayout {
		// 포스트 프로세스 공통 프레임 상수 입력이다. HLSL cbuffer register(b0)에 대응한다.
		static constexpr uint32_t frameDataRegister = 0;
		// 효과별 스칼라 파라미터 입력이다. HLSL cbuffer register(b1)에 대응한다.
		static constexpr uint32_t parameterDataRegister = 1;
		// Vulkan에서는 b1이 descriptor set 1의 binding 0으로 변환된다.
		static constexpr uint32_t parameterDataVulkanBinding = 0;
		// 한 효과가 b1 상수 버퍼에서 사용할 수 있는 스칼라 파라미터 수다.
		static constexpr uint32_t maxParameterCount = 64;
		// scene_depth와 scene_velocity에 기록할 표면의 최소 material 불투명도다.
		static constexpr float surfaceOpacityThreshold = 0.5f;
		// 패키지 pass 하나가 사용할 수 있는 HLSL Texture2D 입력 슬롯 수다.
		static constexpr uint32_t maxTextureCount = 8;
		// 포스트 프로세스 공통 clamp linear sampler다. HLSL SamplerState LinearClamp : register(s0)에 대응한다.
		static constexpr uint32_t linearClampSamplerRegister = 0;
		// SPIR-V binding에서 t4 이후가 공통 sampler binding을 건너뛰도록 실제 binding을 계산한다.
		static constexpr uint32_t ResolveSpirvTextureBinding(const uint32_t slot) {
			return slot < 4 ? slot : slot + 3;
		}
	};
}
