#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"

namespace Chrivent {
	// Vulkan 장면 그래픽 파이프라인의 네이티브 상태 조립과 생성을 담당한다.
	class VulkanGraphicsPipelineBuilder {
	public:
		// 장면 패스가 ViewerVertex에서 사용할 attribute 조합을 구분한다.
		enum class VertexLayout {
			PositionOnly,
			PositionUv,
			Model,
			Velocity
		};

		// 장면 패스 하나의 고정 기능 상태와 attachment 형식을 보관한다.
		struct Configuration {
			VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
			VkFormat colorFormat = VK_FORMAT_UNDEFINED;
			VkFormat depthFormat = VK_FORMAT_UNDEFINED;
			VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
			VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
			VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
			VertexLayout vertexLayout = VertexLayout::Model;
			bool depthBiasEnabled = false;
			bool stencilTestEnabled = false;
			bool depthWriteDisabled = false;
			bool preserveDestinationAlpha = false;
		};

		// 셰이더 프로그램과 고정 기능 설정으로 Vulkan graphics pipeline을 생성한다.
		static GraphicsResult<void> Create(const VulkanDevice& sourceDevice,
			const ShaderProgramDefinition& program, const Configuration& configuration,
			VkPipeline& pipeline);
	};
}
