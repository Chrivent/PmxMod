#pragma once

#include <cstdint>

namespace Chrivent {
	// 모든 렌더링 API가 공유하는 후처리 HLSL 입력 슬롯 계약을 정의한다.
	struct PostProcessInputLayout {
		// 포스트 프로세스 공통 프레임 상수 입력이다. HLSL cbuffer register(b0)에 대응한다.
		static constexpr uint32_t frameDataRegister = 0;
		// 효과별 스칼라 파라미터 입력이다. HLSL cbuffer register(b1)에 대응한다.
		static constexpr uint32_t parameterDataRegister = 1;
		// 한 효과가 b1 상수 버퍼에서 사용할 수 있는 스칼라 파라미터 수다.
		static constexpr uint32_t maxParameterCount = 64;
		// 패키지 pass 하나가 사용할 수 있는 HLSL Texture2D 입력 슬롯 수다.
		static constexpr uint32_t maxTextureCount = 8;
		// 후처리 texture 입력이 시작되는 HLSL register 번호다.
		static constexpr uint32_t firstTextureRegister = 0;
		// 포스트 프로세스가 모든 API에서 제공하는 공통 sampler 개수다.
		static constexpr uint32_t samplerCount = 3;
		// 포스트 프로세스 공통 clamp linear sampler다. HLSL SamplerState LinearClamp : register(s0)에 대응한다.
		static constexpr uint32_t linearClampSamplerRegister = 0;
	};
}
