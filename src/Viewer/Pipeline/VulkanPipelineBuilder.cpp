#include "Viewer/Pipeline/VulkanPipelineBuilder.h"

#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Pipeline/VulkanShaderStageBuilder.h"

#include <cstddef>
#include <iterator>

namespace Chrivent {
	GraphicsError::Result<void> VulkanPipelineBuilder::Create(const VulkanDevice& sourceDevice,
		const ShaderProgramDefinition& program, const Configuration& configuration,
		VkPipeline& pipeline) {
		pipeline = VK_NULL_HANDLE;
		if (sourceDevice.GetDevice() == VK_NULL_HANDLE
			|| configuration.pipelineLayout == VK_NULL_HANDLE
			|| (configuration.colorFormat == VK_FORMAT_UNDEFINED
				&& configuration.depthFormat == VK_FORMAT_UNDEFINED)) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "graphics pipeline 생성",
				"Vulkan graphics pipeline 생성 설정이 올바르지 않습니다"));
		}
		VulkanShaderStageBuilder shaderStages;
		const auto shaderResult = shaderStages.Build(
			sourceDevice, program, configuration.bindingProfile, configuration.invertVertexY);
		if (!shaderResult)
			return std::unexpected(shaderResult.error());
		constexpr VkVertexInputBindingDescription bindingDescription{
			0,
			sizeof(ViewerVertex),
			VK_VERTEX_INPUT_RATE_VERTEX
		};
		VkVertexInputAttributeDescription attributeDescriptions[4]{};
		attributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ViewerVertex, position) };
		uint32_t attributeCount = configuration.vertexLayout == VertexLayout::None ? 0u : 1u;
		if (configuration.vertexLayout == VertexLayout::PositionUv) {
			attributeDescriptions[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ViewerVertex, uv) };
			attributeCount = 2;
		} else if (configuration.vertexLayout == VertexLayout::Model
			|| configuration.vertexLayout == VertexLayout::Edge
			|| configuration.vertexLayout == VertexLayout::Velocity) {
			attributeDescriptions[1] = {
				1,
				0,
				VK_FORMAT_R32G32B32_SFLOAT,
				static_cast<uint32_t>(
					configuration.vertexLayout == VertexLayout::Velocity
						? offsetof(ViewerVertex, previousPosition)
						: offsetof(ViewerVertex, normal))
			};
			attributeDescriptions[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ViewerVertex, uv) };
			attributeCount = 3;
			if (configuration.vertexLayout == VertexLayout::Edge)
				attributeDescriptions[2] = { 2, 0, VK_FORMAT_R32_SFLOAT, offsetof(ViewerVertex, edgeScale) };
			else if (configuration.vertexLayout == VertexLayout::Model) {
				attributeDescriptions[3] = { 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ViewerVertex, subTextureUv) };
				attributeCount = 4;
			}
		}
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = configuration.vertexLayout == VertexLayout::None ? 0u : 1u;
		vertexInputInfo.pVertexBindingDescriptions = configuration.vertexLayout == VertexLayout::None
			? nullptr : &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = attributeCount;
		vertexInputInfo.pVertexAttributeDescriptions = attributeCount == 0 ? nullptr : attributeDescriptions;
		constexpr VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			nullptr,
			0,
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};
		constexpr VkPipelineViewportStateCreateInfo viewportState{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			nullptr,
			0,
			1,
			nullptr,
			1
		};
		static constexpr VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		constexpr VkPipelineDynamicStateCreateInfo dynamicState{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			nullptr,
			0,
			std::size(dynamicStates),
			dynamicStates
		};
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = configuration.cullMode;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = configuration.depthBiasEnabled ? VK_TRUE : VK_FALSE;
		rasterizer.depthBiasConstantFactor = configuration.depthBiasEnabled ? -1.0f : 0.0f;
		rasterizer.depthBiasSlopeFactor = configuration.depthBiasEnabled ? -1.0f : 0.0f;
		rasterizer.lineWidth = 1.0f;
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = configuration.sampleCount;
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = configuration.depthWriteDisabled ? VK_FALSE : VK_TRUE;
		depthStencil.depthCompareOp = configuration.depthCompareOp;
		depthStencil.stencilTestEnable = configuration.stencilTestEnabled ? VK_TRUE : VK_FALSE;
		if (configuration.stencilTestEnabled) {
			constexpr VkStencilOpState stencilState{
				VK_STENCIL_OP_KEEP,
				VK_STENCIL_OP_REPLACE,
				VK_STENCIL_OP_KEEP,
				VK_COMPARE_OP_NOT_EQUAL,
				1,
				1,
				1
			};
			depthStencil.front = stencilState;
			depthStencil.back = stencilState;
		}
		const bool hasColorAttachment = configuration.colorFormat != VK_FORMAT_UNDEFINED;
		const bool hasDepthAttachment = configuration.depthFormat != VK_FORMAT_UNDEFINED;
		const bool velocityInput = configuration.vertexLayout == VertexLayout::Velocity;
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.blendEnable = configuration.blendEnabled && !velocityInput ? VK_TRUE : VK_FALSE;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.srcAlphaBlendFactor = configuration.preserveDestinationAlpha
			? VK_BLEND_FACTOR_ZERO : VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstAlphaBlendFactor = configuration.preserveDestinationAlpha
			? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.attachmentCount = hasColorAttachment ? 1u : 0u;
		colorBlending.pAttachments = hasColorAttachment ? &blendAttachment : nullptr;
		const bool depthHasStencil = configuration.depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT
			|| configuration.depthFormat == VK_FORMAT_D24_UNORM_S8_UINT;
		VkPipelineRenderingCreateInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount = hasColorAttachment ? 1u : 0u;
		renderingInfo.pColorAttachmentFormats = hasColorAttachment ? &configuration.colorFormat : nullptr;
		renderingInfo.depthAttachmentFormat = configuration.depthFormat;
		renderingInfo.stencilAttachmentFormat = hasDepthAttachment && depthHasStencil
			? configuration.depthFormat : VK_FORMAT_UNDEFINED;
		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		createInfo.pNext = &renderingInfo;
		createInfo.stageCount = VulkanShaderStageBuilder::stageCount;
		createInfo.pStages = shaderStages.GetStages();
		createInfo.pVertexInputState = &vertexInputInfo;
		createInfo.pInputAssemblyState = &inputAssembly;
		createInfo.pViewportState = &viewportState;
		createInfo.pRasterizationState = &rasterizer;
		createInfo.pMultisampleState = &multisampling;
		createInfo.pDepthStencilState = hasDepthAttachment ? &depthStencil : nullptr;
		createInfo.pColorBlendState = hasColorAttachment ? &colorBlending : nullptr;
		createInfo.pDynamicState = &dynamicState;
		createInfo.layout = configuration.pipelineLayout;
		const VkResult result = vkCreateGraphicsPipelines(sourceDevice.GetDevice(),
			VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
		if (result == VK_SUCCESS)
			return {};
		return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
			GraphicsErrorCode::ResourceCreationFailed, "graphics pipeline 생성",
			"Vulkan graphics pipeline을 만들지 못했습니다", result, true));
	}
}
