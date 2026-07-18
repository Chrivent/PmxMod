#pragma once

#include "Viewer/Shader/ShaderProgramDefinition.h"

#include <cstdint>
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
		EffectResourceLifetime lifetime = EffectResourceLifetime::Transient;
		EffectTextureFormat format = EffectTextureFormat::Rgba16Float;
		EffectPassResolution resolution = EffectPassResolution::Full;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	// 후처리 패스 입력이 참조하는 장면 또는 효과 리소스 종류를 구분한다.
	enum class EffectPassInputKind {
		EffectInput,
		SceneColor,
		SceneDepth,
		SceneVelocity,
		Resource
	};

	// 효과 패스의 texture 슬롯과 정규화된 입력 리소스를 연결한다.
	struct EffectPassInputDefinition {
		uint32_t slot = 0;
		EffectPassInputKind kind = EffectPassInputKind::EffectInput;
		size_t resourceIndex = 0;
	};

	// 후처리 패스가 최종 화면과 효과 리소스 중 어디에 출력할지 구분한다.
	enum class EffectPassOutputKind {
		EffectOutput,
		Resource
	};

	// 효과 패스의 정규화된 출력 리소스를 나타낸다.
	struct EffectPassOutputDefinition {
		EffectPassOutputKind kind = EffectPassOutputKind::EffectOutput;
		size_t resourceIndex = 0;
	};

	// 셰이더 프로그램과 정규화된 입출력으로 구성된 후처리 패스를 나타낸다.
	struct EffectPassDefinition {
		ShaderProgramDefinition program;
		std::vector<EffectPassInputDefinition> inputs;
		EffectPassOutputDefinition output;
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
		std::vector<EffectParameterDefinition> parameters;
		std::vector<EffectResourceDefinition> resources;
		std::vector<EffectPassDefinition> passes;
	};

}
