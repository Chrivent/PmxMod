#include "Viewer/PostProcess/VulkanPostProcessPipelines.h"

#include "Viewer/Pipeline/VulkanShaderStageBuilder.h"

#include <iostream>

namespace Chrivent {
	bool VulkanPostProcessPipelines::CreateGraphicsPipeline(const VulkanDevice& sourceDevice,
		const VkPipelineLayout pipelineLayout, const ShaderProgramDefinition& program,
		const VkFormat targetFormat, VkPipeline& pipeline) const {
		std::string error;
		VulkanShaderStageBuilder shaderStages;
		if (!shaderStages.Build(sourceDevice, program, SpirvBindingProfile::PostProcess, error, true)) {
			if (!error.empty())
				std::cerr << error << '\n';
			return false;
		}
		VkPipelineVertexInputStateCreateInfo vertexInput{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
		};
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};
		VkPipelineViewportStateCreateInfo viewportState{
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
			.polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f
		};
		VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};
		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo blending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1, .pAttachments = &blendAttachment
		};
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1, .pColorAttachmentFormats = &targetFormat
		};
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = &renderingInfo;
		pipelineInfo.stageCount = VulkanShaderStageBuilder::stageCount;
		pipelineInfo.pStages = shaderStages.GetStages();
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &blending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
			&pipelineInfo, nullptr, &pipeline) == VK_SUCCESS;
	}

	VulkanPostProcessPipelines::~VulkanPostProcessPipelines() {
		Reset();
	}

	bool VulkanPostProcessPipelines::Initialize(const VulkanDevice& sourceDevice,
		const VkPipelineLayout pipelineLayout, const std::span<const ShaderProgramDefinition> programs,
		const std::span<const VkFormat> targetFormats) {
		Reset();
		device = sourceDevice.GetDevice();
		if (programs.empty())
			return true;
		if (device == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE
			|| programs.size() != targetFormats.size())
			return false;
		for (size_t index = 0; index < programs.size(); index++) {
			VkPipeline pipeline = VK_NULL_HANDLE;
			if (!CreateGraphicsPipeline(sourceDevice, pipelineLayout,
				programs[index], targetFormats[index], pipeline)) {
				Reset();
				return false;
			}
			pipelines.emplace_back(pipeline);
		}
		return true;
	}

	void VulkanPostProcessPipelines::Swap(VulkanPostProcessPipelines& other) noexcept {
		std::swap(device, other.device);
		pipelines.swap(other.pipelines);
	}

	void VulkanPostProcessPipelines::Reset() {
		if (device != VK_NULL_HANDLE) {
			for (const VkPipeline pipeline : pipelines) {
				if (pipeline != VK_NULL_HANDLE)
					vkDestroyPipeline(device, pipeline, nullptr);
			}
		}
		pipelines.clear();
		device = VK_NULL_HANDLE;
	}
}
