#pragma once

#include <cstdint>

namespace Chrivent {
	// 내장 장면 셰이더가 구현해야 하는 고정 ABI 프로필을 구분한다.
	enum class SceneShaderAbi {
		None,
		ModelV1,
		EdgeV1,
		GroundShadowV1
	};

	// 모든 API가 공유하는 장면 셰이더 상수·텍스처·샘플러 슬롯 계약을 정의한다.
	struct SceneShaderInputLayout {
		static constexpr auto modelAbi = "pmxmod.scene.model.v1";
		static constexpr auto edgeAbi = "pmxmod.scene.edge.v1";
		static constexpr auto groundShadowAbi = "pmxmod.scene.ground_shadow.v1";
		static constexpr uint32_t vertexConstantRegister = 0;
		static constexpr uint32_t pixelConstantRegister = 1;
		static constexpr uint32_t baseTextureRegister = 0;
		static constexpr uint32_t toonTextureRegister = 1;
		static constexpr uint32_t sphereTextureRegister = 2;
		static constexpr uint32_t textureCount = 3;
		static constexpr uint32_t baseSamplerRegister = 0;
		static constexpr uint32_t toonSamplerRegister = 1;
		static constexpr uint32_t sphereSamplerRegister = 2;
		static constexpr uint32_t samplerCount = 3;
	};
}
