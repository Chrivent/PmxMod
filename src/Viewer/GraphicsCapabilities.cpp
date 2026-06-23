#include "Viewer/GraphicsCapabilities.h"

#include <iostream>

namespace Chrivent {
	void GraphicsCapabilities::Print() const {
		std::cout << "graphics_api=" << apiName << '\n';
		std::cout << "graphics_api_version=" << apiVersion << '\n';
		std::cout << "graphics_shader_version=" << shaderVersion << '\n';
		std::cout << "graphics_gpu=" << gpuName << '\n';
		std::cout << "graphics_gpu_type=" << gpuType << '\n';
		std::cout << "graphics_max_samples=" << maxSampleCount << '\n';
		std::cout << "graphics_uniform_alignment=" << uniformBufferAlignment << '\n';
		std::cout << "graphics_max_texture_bindings=" << maxTextureBindings << '\n';
		std::cout << "graphics_timeline_sync=" << supportsTimelineSynchronization << '\n';
		std::cout << "graphics_dynamic_rendering=" << supportsDynamicRendering << '\n';
		std::cout << "graphics_enhanced_barriers=" << supportsEnhancedBarriers << '\n';
	}
}
