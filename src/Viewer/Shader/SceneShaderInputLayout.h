#pragma once

#include <cstdint>

namespace Chrivent {
	// 모든 API가 공유하는 장면 셰이더 상수·텍스처·샘플러 슬롯 계약을 정의한다.
	struct SceneShaderInputLayout {
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
