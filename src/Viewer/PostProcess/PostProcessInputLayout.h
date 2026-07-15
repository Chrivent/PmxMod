#pragma once

#include "Viewer/Shader/SpirvBindingLayout.h"

#include <cstdint>

namespace Chrivent {
	// 모든 렌더링 API가 공유하는 후처리 HLSL 입력 슬롯 계약을 정의한다.
	struct PostProcessInputLayout {
		// 포스트 프로세스 공통 프레임 상수 입력이다. HLSL cbuffer register(b0)에 대응한다.
		static constexpr uint32_t frameDataRegister = SpirvBindingLayout::frameDataRegister;
		// Vulkan에서 공통 프레임 상수가 배치되는 descriptor set과 binding이다.
		static constexpr uint32_t frameDataVulkanSet = SpirvBindingLayout::frameDataSet;
		static constexpr uint32_t frameDataVulkanBinding = SpirvBindingLayout::frameDataBinding;
		// 효과별 스칼라 파라미터 입력이다. HLSL cbuffer register(b1)에 대응한다.
		static constexpr uint32_t parameterDataRegister = SpirvBindingLayout::parameterDataRegister;
		// Vulkan에서 효과별 파라미터가 배치되는 descriptor set과 binding이다.
		static constexpr uint32_t parameterDataVulkanSet = SpirvBindingLayout::parameterDataSet;
		static constexpr uint32_t parameterDataVulkanBinding = SpirvBindingLayout::parameterDataBinding;
		// 한 효과가 b1 상수 버퍼에서 사용할 수 있는 스칼라 파라미터 수다.
		static constexpr uint32_t maxParameterCount = 64;
		// scene_depth와 scene_velocity에 기록할 표면의 최소 material 불투명도다.
		static constexpr float surfaceOpacityThreshold = 0.5f;
		// 패키지 pass 하나가 사용할 수 있는 HLSL Texture2D 입력 슬롯 수다.
		static constexpr uint32_t maxTextureCount = SpirvBindingLayout::postProcessTextureCount;
		// Vulkan에서 texture와 sampler가 배치되는 descriptor set이다.
		static constexpr uint32_t textureVulkanSet = SpirvBindingLayout::textureSet;
		// Vulkan 후처리 pipeline layout이 사용하는 descriptor set 개수다.
		static constexpr uint32_t vulkanSetCount = textureVulkanSet + 1;
		// 포스트 프로세스가 모든 API에서 제공하는 공통 sampler 개수다.
		static constexpr uint32_t samplerCount = SpirvBindingLayout::samplerCount;
		// 포스트 프로세스 공통 clamp linear sampler다. HLSL SamplerState LinearClamp : register(s0)에 대응한다.
		static constexpr uint32_t linearClampSamplerRegister = 0;
		// SPIR-V binding에서 t4 이후가 공통 sampler binding을 건너뛰도록 실제 binding을 계산한다.
		static constexpr uint32_t ResolveSpirvTextureBinding(const uint32_t slot) {
			return SpirvBindingLayout::ResolveTextureBinding(slot);
		}
		// SPIR-V에서 공통 sampler 슬롯에 대응하는 binding을 계산한다.
		static constexpr uint32_t ResolveSpirvSamplerBinding(const uint32_t slot) {
			return SpirvBindingLayout::ResolveSamplerBinding(slot);
		}
	};
}
