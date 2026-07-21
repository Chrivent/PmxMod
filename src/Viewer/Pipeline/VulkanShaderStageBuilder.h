#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/SpirvBindingLayout.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"

#include <span>
#include <string>

namespace Chrivent {
	// HLSL pass를 Vulkan shader module과 graphics pipeline stage 정보로 구성한다.
	class VulkanShaderStageBuilder {
		// stage builder에서만 사용하는 Vulkan shader module의 수명을 관리한다.
		class ShaderModule {
			VkShaderModule shaderModule = VK_NULL_HANDLE;
			VkDevice device = VK_NULL_HANDLE;

		public:
			ShaderModule() = default;
			~ShaderModule();

			ShaderModule(const ShaderModule&) = delete;
			ShaderModule& operator=(const ShaderModule&) = delete;

			VkShaderModule GetShaderModule() const { return shaderModule; }

			// SPIR-V 바이트 코드에서 Vulkan shader module을 생성한다.
			GraphicsError::Result<void> Initialize(
				const VulkanDevice& sourceDevice, std::span<const uint32_t> spvBytes);
			// 생성한 shader module 리소스를 해제한다.
			void Reset();
		};

		ShaderModule vertexShader;
		ShaderModule pixelShader;
		std::string vertexEntry;
		std::string pixelEntry;
		VkPipelineShaderStageCreateInfo stages[2]{};

		// shader module과 진입점 이름을 Vulkan stage 생성 정보로 연결한다.
		void BuildStageDescriptions();

	public:
		static constexpr uint32_t stageCount = 2;

		const VkPipelineShaderStageCreateInfo* GetStages() const { return stages; }

		// 패키지 pass를 Vulkan용 SPIR-V로 컴파일하고 stage 정보를 생성한다.
		GraphicsError::Result<void> Build(const VulkanDevice& sourceDevice,
			const ShaderProgramDefinition& program, SpirvBindingProfile bindingProfile,
			bool invertVertexY = false);
	};
}
