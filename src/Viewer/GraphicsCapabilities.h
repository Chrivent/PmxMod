#pragma once

#include <cstdint>
#include <string>

namespace Chrivent {
	struct GraphicsCapabilities {
		std::string apiName;
		std::string apiVersion;
		std::string shaderVersion;
		std::string gpuName;
		std::string gpuType;
		uint32_t maxSampleCount = 1;
		uint64_t uniformBufferAlignment = 1;
		uint32_t maxTextureBindings = 0;
		uint32_t shaderModelMajor = 0;
		uint32_t shaderModelMinor = 0;
		bool supportsTimelineSynchronization = false;
		bool supportsDynamicRendering = false;
		bool supportsEnhancedBarriers = false;

		// 감지된 그래픽 API와 GPU 기능을 공통 로그 형식으로 출력한다.
		void Print() const;
	};
}
