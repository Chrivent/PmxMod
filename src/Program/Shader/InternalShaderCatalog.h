#pragma once

#include "Viewer/Shader/SceneShaderRuntimeContract.h"

#include <expected>
#include <filesystem>
#include <string>

namespace Chrivent {
	// 엔진 내부 HLSL 파일을 API가 소비할 역할별 패스 계약으로 구성한다.
	class InternalShaderCatalog {
		// internal 셰이더 디렉터리 아래에서 필수 HLSL 파일을 확인한다.
		static bool ResolveShaderPath(const std::filesystem::path& shaderDirectory,
			const std::filesystem::path& relativePath, std::filesystem::path& shaderPath,
			std::string& error);
		// 셰이더 파일과 진입점을 단일 패스 계약으로 구성한다.
		static ShaderProgramDefinition CreateProgram(std::filesystem::path shaderPath,
			const char* vertexEntry, const char* pixelEntry);

	public:
		// 모델·엣지·지면 그림자와 후처리 장면 입력용 내부 패스를 모두 불러온다.
		static std::expected<SceneShaderRuntimeContract, std::string> Load(
			const std::filesystem::path& shaderDirectory,
			bool invertNdcYForTextureCoordinates);
	};
}
