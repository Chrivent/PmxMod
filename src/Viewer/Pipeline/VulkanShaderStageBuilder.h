#pragma once

#include "Viewer/Shader/SpirvBindingLayout.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"
#include "Viewer/Shader/VulkanShaderModule.h"

#include <string>

namespace Chrivent {
	// HLSL pass를 Vulkan shader module과 graphics pipeline stage 정보로 구성한다.
	class VulkanShaderStageBuilder {
		VulkanShaderModule vertexShader;
		VulkanShaderModule pixelShader;
		std::string vertexEntry;
		std::string pixelEntry;
		VkPipelineShaderStageCreateInfo stages[2]{};

		// shader module과 진입점 이름을 Vulkan stage 생성 정보로 연결한다.
		void BuildStageDescriptions();

	public:
		static constexpr uint32_t stageCount = 2;

		const VkPipelineShaderStageCreateInfo* GetStages() const { return stages; }

		// 패키지 pass를 Vulkan용 SPIR-V로 컴파일하고 stage 정보를 생성한다.
		bool Build(const VulkanDevice& sourceDevice, const ShaderProgramDefinition& program,
			SpirvBindingProfile bindingProfile, std::string& outError, bool invertVertexY = false);
	};
}
