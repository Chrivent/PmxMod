#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

namespace Chrivent {
	// 장면과 후처리에서 공유하는 Vulkan 그래픽 파이프라인 상태를 조립한다.
	class VulkanPipelineBuilder {
	public:
		// 패스가 사용할 vertex 입력 attribute 조합을 구분한다.
		enum class VertexLayout {
			None,
			PositionOnly,
			PositionUv,
			Model,
			Velocity
		};

		// 그래픽 패스 하나의 셰이더 binding과 고정 기능 상태를 보관한다.
		struct Configuration {
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			VkFormat colorFormat = VK_FORMAT_UNDEFINED;
			VkFormat depthFormat = VK_FORMAT_UNDEFINED;
			VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
			VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
			VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
			VertexLayout vertexLayout = VertexLayout::Model;
			SpirvBindingProfile bindingProfile = SpirvBindingProfile::Scene;
			bool invertVertexY = false;
			bool blendEnabled = true;
			bool depthBiasEnabled = false;
			bool stencilTestEnabled = false;
			bool depthWriteDisabled = false;
			bool preserveDestinationAlpha = false;
		};

		// 셰이더 프로그램과 고정 기능 설정으로 Vulkan graphics pipeline을 생성한다.
		static GraphicsError::Result<void> Create(const VulkanDevice& sourceDevice,
			const ShaderProgramDefinition& program, const Configuration& configuration,
			VkPipeline& pipeline);
	};
}
