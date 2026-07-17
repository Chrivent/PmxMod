#pragma once

#include "Viewer/PostProcess/PostProcessInputLayout.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"

#include <cstdint>

namespace Chrivent {
	// SPIR-V를 생성할 때 적용할 렌더링 입력 계약을 구분한다.
	enum class SpirvBindingProfile {
		Scene,
		PostProcess
	};

	// 장면 및 후처리 HLSL register를 Vulkan descriptor binding으로 변환하는 공통 규칙이다.
	struct SpirvBindingLayout {
		static constexpr uint32_t frameDataSet = 0;
		static constexpr uint32_t frameDataBinding = 0;
		static constexpr uint32_t parameterDataSet = 1;
		static constexpr uint32_t parameterDataBinding = 0;
		static constexpr uint32_t textureSet = 2;
		static constexpr uint32_t setCount = textureSet + 1;

		// 선택한 입력 계약의 첫 번째 constant buffer register를 반환한다.
		static constexpr uint32_t ResolveFrameDataRegister(const SpirvBindingProfile profile) {
			return profile == SpirvBindingProfile::Scene
				? SceneShaderInputLayout::vertexConstantRegister : PostProcessInputLayout::frameDataRegister;
		}
		// 선택한 입력 계약의 두 번째 constant buffer register를 반환한다.
		static constexpr uint32_t ResolveParameterDataRegister(const SpirvBindingProfile profile) {
			return profile == SpirvBindingProfile::Scene
				? SceneShaderInputLayout::pixelConstantRegister : PostProcessInputLayout::parameterDataRegister;
		}
		// 선택한 입력 계약이 허용하는 texture register 수를 반환한다.
		static constexpr uint32_t ResolveTextureCount(const SpirvBindingProfile profile) {
			return profile == SpirvBindingProfile::Scene
				? SceneShaderInputLayout::textureCount : PostProcessInputLayout::maxTextureCount;
		}
		// 선택한 입력 계약이 허용하는 sampler register 수를 반환한다.
		static constexpr uint32_t ResolveSamplerCount(const SpirvBindingProfile profile) {
			return profile == SpirvBindingProfile::Scene
				? SceneShaderInputLayout::samplerCount : PostProcessInputLayout::samplerCount;
		}
		// texture register가 공통 sampler binding을 침범하지 않도록 SPIR-V binding을 계산한다.
		static constexpr uint32_t ResolveTextureBinding(const uint32_t slot) { return slot < 4 ? slot : slot + 3; }
		// sampler register에 대응하는 SPIR-V binding을 계산한다.
		static constexpr uint32_t ResolveSamplerBinding(const uint32_t slot) { return 4 + slot; }
	};
}
