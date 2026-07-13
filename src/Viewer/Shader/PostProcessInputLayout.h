#pragma once

#include <cstdint>

namespace Chrivent {
	struct PostProcessInputLayout {
		// 포스트 프로세스 공통 프레임 상수 입력이다. HLSL cbuffer register(b0)에 대응한다.
		static constexpr uint32_t frameDataRegister = 0;
		// 패키지 pass 하나가 사용할 수 있는 HLSL Texture2D 입력 슬롯 수다.
		static constexpr uint32_t maxTextureCount = 8;
		// 포스트 프로세스 공통 clamp linear sampler다. HLSL SamplerState LinearClamp : register(s0)에 대응한다.
		static constexpr uint32_t linearClampSamplerRegister = 0;
		// DXC SPIR-V binding에서 t4 이후가 공통 sampler binding을 건너뛰도록 실제 binding을 계산한다.
		static constexpr uint32_t ResolveSpirvTextureBinding(const uint32_t slot) {
			return slot < 4 ? slot : slot + 3;
		}
	};
}
