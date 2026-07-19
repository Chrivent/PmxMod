#include "Viewer/Pipeline/VulkanGraphicsPipelineBuilder.h"

#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Pipeline/VulkanShaderStageBuilder.h"

#include <cstddef>
#include <iterator>

namespace Chrivent {
	GraphicsError::Result<void> VulkanGraphicsPipelineBuilder::Create(const VulkanDevice& sourceDevice,
		const ShaderProgramDefinition& program, const Configuration& configuration,
		VkPipeline& pipeline) {
		pipeline = VK_NULL_HANDLE;
		if (sourceDevice.GetDevice() == VK_NULL_HANDLE
			|| configuration.pipelineLayout == VK_NULL_HANDLE
			|| configuration.depthFormat == VK_FORMAT_UNDEFINED) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "graphics pipeline 생성",
				"Vulkan graphics pipeline 생성 설정이 올바르지 않습니다"));
		}
		VulkanShaderStageBuilder shaderStages;
		const auto shaderResult = shaderStages.Build(
			sourceDevice, program, SpirvBindingProfile::Scene);
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		constexpr VkVertexInputBindingDescription bindingDescription{
			.binding = 0,
			.stride = sizeof(ViewerVertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};
		VkVertexInputAttributeDescription attributeDescriptions[3]{};
		attributeDescriptions[0] = {
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(ViewerVertex, position)
		};
		uint32_t attributeCount = 1;
		if (configuration.vertexLayout == VertexLayout::PositionUv) {
			attributeDescriptions[1] = {
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(ViewerVertex, uv)
			};
			attributeCount = 2;
		} else if (configuration.vertexLayout == VertexLayout::Model
			|| configuration.vertexLayout == VertexLayout::Velocity) {
			attributeDescriptions[1] = {
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = static_cast<uint32_t>(
					configuration.vertexLayout == VertexLayout::Velocity
						? offsetof(ViewerVertex, previousPosition)
						: offsetof(ViewerVertex, normal))
			};
			attributeDescriptions[2] = {
				.location = 2,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(ViewerVertex, uv)
			};
			attributeCount = 3;
		}
		const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescription,
			.vertexAttributeDescriptionCount = attributeCount,
			.pVertexAttributeDescriptions = attributeDescriptions
		};
		constexpr VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};
		constexpr VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1
		};
		static constexpr VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		constexpr VkPipelineDynamicStateCreateInfo dynamicState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = std::size(dynamicStates),
			.pDynamicStates = dynamicStates
		};
		VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = configuration.cullMode,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = configuration.depthBiasEnabled ? VK_TRUE : VK_FALSE,
			.depthBiasConstantFactor = configuration.depthBiasEnabled ? -1.0f : 0.0f,
			.depthBiasSlopeFactor = configuration.depthBiasEnabled ? -1.0f : 0.0f,
			.lineWidth = 1.0f
		};
		const VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = configuration.sampleCount
		};
		VkPipelineDepthStencilStateCreateInfo depthStencil{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = configuration.depthWriteDisabled ? VK_FALSE : VK_TRUE,
			.depthCompareOp = configuration.depthCompareOp,
			.stencilTestEnable = configuration.stencilTestEnabled ? VK_TRUE : VK_FALSE
		};
		if (configuration.stencilTestEnabled) {
			constexpr VkStencilOpState stencilState{
				.failOp = VK_STENCIL_OP_KEEP,
				.passOp = VK_STENCIL_OP_REPLACE,
				.depthFailOp = VK_STENCIL_OP_KEEP,
				.compareOp = VK_COMPARE_OP_NOT_EQUAL,
				.compareMask = 1,
				.writeMask = 1,
				.reference = 1
			};
			depthStencil.front = stencilState;
			depthStencil.back = stencilState;
		}
		const bool hasColorAttachment = configuration.colorFormat != VK_FORMAT_UNDEFINED;
		const bool velocityInput = configuration.vertexLayout == VertexLayout::Velocity;
		VkPipelineColorBlendAttachmentState blendAttachment{
			.blendEnable = velocityInput ? VK_FALSE : VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = configuration.preserveDestinationAlpha
				? VK_BLEND_FACTOR_ZERO : VK_BLEND_FACTOR_SRC_ALPHA,
			.dstAlphaBlendFactor = configuration.preserveDestinationAlpha
				? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
				| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
		const VkPipelineColorBlendStateCreateInfo colorBlending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = hasColorAttachment ? 1u : 0u,
			.pAttachments = hasColorAttachment ? &blendAttachment : nullptr
		};
		const bool depthHasStencil = configuration.depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT
			|| configuration.depthFormat == VK_FORMAT_D24_UNORM_S8_UINT;
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = hasColorAttachment ? 1u : 0u,
			.pColorAttachmentFormats = hasColorAttachment ? &configuration.colorFormat : nullptr,
			.depthAttachmentFormat = configuration.depthFormat,
			.stencilAttachmentFormat = hasColorAttachment && depthHasStencil
				? configuration.depthFormat : VK_FORMAT_UNDEFINED
		};
		VkGraphicsPipelineCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderingInfo,
			.stageCount = VulkanShaderStageBuilder::stageCount,
			.pStages = shaderStages.GetStages(),
			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = &depthStencil,
			.pColorBlendState = hasColorAttachment ? &colorBlending : nullptr,
			.pDynamicState = &dynamicState,
			.layout = configuration.pipelineLayout
		};
		const VkResult result = vkCreateGraphicsPipelines(sourceDevice.GetDevice(),
			VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
		if (result == VK_SUCCESS)
			return {};
		return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
			GraphicsErrorCode::ResourceCreationFailed, "graphics pipeline 생성",
			"Vulkan graphics pipeline을 만들지 못했습니다", result, true));
	}
}
