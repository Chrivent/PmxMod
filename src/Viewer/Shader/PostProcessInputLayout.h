#pragma once

#include <cstdint>

namespace Chrivent {
	struct PostProcessInputLayout {
		// 포스트 프로세스 공통 프레임 상수 입력이다. HLSL cbuffer register(b0)에 대응한다.
		static constexpr uint32_t frameDataRegister = 0;
		// 포스트 프로세스 공통 색상 입력이다. HLSL Texture2D SceneColor : register(t0)에 대응한다.
		static constexpr uint32_t sceneColorRegister = 0;
		// 포스트 프로세스 공통 깊이 입력이다. HLSL Texture2D SceneDepth : register(t1)에 대응한다.
		static constexpr uint32_t sceneDepthRegister = 1;
		// 포스트 프로세스 공통 초점 히스토리 입력이다. HLSL Texture2D FocusHistory : register(t2)에 대응한다.
		static constexpr uint32_t focusHistoryRegister = 2;
		// 현재 effect가 시작될 때의 색상 입력이다. HLSL Texture2D EffectSourceColor : register(t3)에 대응한다.
		static constexpr uint32_t effectSourceColorRegister = 3;
		// 포스트 프로세스 공통 clamp linear sampler다. HLSL SamplerState LinearClamp : register(s0)에 대응한다.
		static constexpr uint32_t linearClampSamplerRegister = 0;
		// 모든 API가 최종적으로 노출해야 하는 포스트 프로세스 SRV 개수다.
		static constexpr uint32_t requiredTextureCount = 4;
	};
}
