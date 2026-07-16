#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Chrivent {
	// 후처리 리소스의 화면 대비 해상도 규칙을 구분한다.
	enum class EffectPassResolution {
		Full,
		Half,
		Quarter,
		Eighth,
		Fixed
	};

	// 후처리 중간 리소스가 사용할 색상 형식을 구분한다.
	enum class EffectTextureFormat {
		Rgba8Unorm,
		Rgba16Float,
		Rgba32Float
	};

	// 후처리 리소스를 현재 프레임 전용과 프레임 간 history로 구분한다.
	enum class EffectResourceLifetime {
		Transient,
		History
	};

	// 효과가 생성할 중간 또는 history texture의 실행 계약을 나타낸다.
	struct EffectResourceDefinition {
		std::string name;
		EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
		EffectTextureFormat format = EffectTextureFormat::Rgba16Float;
		EffectPassResolution resolution = EffectPassResolution::Full;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// 효과 패스의 texture 슬롯과 참조할 리소스 이름을 연결한다.
	struct EffectPassInputDefinition {
		uint32_t slot = 0;
		std::string resource;
	};

	// 셰이더 파일과 진입점 및 입출력으로 구성된 렌더링 패스를 나타낸다.
	struct EffectPassDefinition {
		std::string name;
		std::filesystem::path shaderPath;
		std::string vertexEntry = "VSMain";
		std::string pixelEntry = "PSMain";
		std::vector<EffectPassInputDefinition> inputs;
		std::string output;
	};

	// 효과가 b1 상수 버퍼에서 사용할 스칼라 파라미터 한 개를 선언한다.
	struct EffectParameterDefinition {
		std::string id;
		std::string name;
		uint32_t slot = 0;
		float defaultValue = 0.0f;
		float minimumValue = 0.0f;
		float maximumValue = 1.0f;
	};

	// API가 소비하는 후처리 리소스와 패스 실행 계약을 나타낸다.
	struct EffectRuntimeDefinition {
		std::vector<std::string> inputs;
		std::vector<EffectParameterDefinition> parameters;
		std::vector<EffectResourceDefinition> resources;
		std::vector<EffectPassDefinition> passes;
	};

	// 내장 모델 렌더링이 요구하는 표면·외곽선·지면 그림자 패스를 보관한다.
	struct BuiltInShaderPasses {
		EffectPassDefinition model;
		EffectPassDefinition edge;
		EffectPassDefinition groundShadow;
	};

	// API 구현에서 파일명과 진입점을 알지 않도록 엔진 장면 입력 패스를 역할별로 보관한다.
	struct SceneInputShaderPasses {
		EffectPassDefinition depth;
		EffectPassDefinition velocity;
		EffectPassDefinition velocityInvertedY;
	};
}
