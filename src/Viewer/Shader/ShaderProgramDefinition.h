#pragma once

#include <filesystem>
#include <string>

namespace Chrivent {
	// API가 컴파일할 HLSL 파일과 vertex 및 pixel 진입점을 지정한다.
	struct ShaderProgramDefinition {
		std::filesystem::path shaderPath;
		std::filesystem::path includeRoot;
		std::string vertexEntry = "VSMain";
		std::string pixelEntry = "PSMain";
	};
}
