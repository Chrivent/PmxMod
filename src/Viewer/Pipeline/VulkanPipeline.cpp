#include "Viewer/Pipeline/VulkanPipeline.h"

#include "Viewer/Pipeline/VulkanShaderStageBuilder.h"
#include "Viewer/Geometry/ViewerGeometry.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <cstddef>
#include <iostream>

namespace Chrivent {
	bool VulkanPipeline::CreateDescriptorSetLayouts() {
		constexpr VkDescriptorSetLayoutBinding vertexConstantBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = nullptr
		};
		VkDescriptorSetLayoutCreateInfo vertexLayoutInfo{};
		vertexLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		vertexLayoutInfo.bindingCount = 1;
		vertexLayoutInfo.pBindings = &vertexConstantBinding;
		if (vkCreateDescriptorSetLayout(device, &vertexLayoutInfo, nullptr, &descriptorSetLayouts[0]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan vertex descriptor set layout.\n";
			return false;
		}
		constexpr VkDescriptorSetLayoutBinding pixelConstantBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};
		VkDescriptorSetLayoutCreateInfo pixelLayoutInfo{};
		pixelLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		pixelLayoutInfo.bindingCount = 1;
		pixelLayoutInfo.pBindings = &pixelConstantBinding;
		if (vkCreateDescriptorSetLayout(device, &pixelLayoutInfo, nullptr, &descriptorSetLayouts[1]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan pixel descriptor set layout.\n";
			return false;
		}
		constexpr VkDescriptorSetLayoutBinding textureBindings[] = {
			VkDescriptorSetLayoutBinding{
				.binding = SceneShaderInputLayout::baseTextureRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			},
			VkDescriptorSetLayoutBinding{
				.binding = SceneShaderInputLayout::toonTextureRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			},
			VkDescriptorSetLayoutBinding{
				.binding = SceneShaderInputLayout::sphereTextureRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			},
			VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::baseSamplerRegister),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			},
			VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::toonSamplerRegister),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			},
			VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::sphereSamplerRegister),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			}
		};
		VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
		textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		textureLayoutInfo.bindingCount = std::size(textureBindings);
		textureLayoutInfo.pBindings = textureBindings;
		if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr, &descriptorSetLayouts[2]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan texture descriptor set layout.\n";
			return false;
		}
		return true;
	}

	bool VulkanPipeline::CreatePipelineLayout() {
		VkPipelineLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		createInfo.setLayoutCount = 3;
		createInfo.pSetLayouts = descriptorSetLayouts;
		if (vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan pipeline layout.\n";
			return false;
		}
		return true;
	}

	bool VulkanPipeline::CreateGraphicsPipelines(const VulkanDevice& sourceDevice,
		const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, const BuiltInShaderPasses& passes,
		const EffectPassDefinition& depthPass, const EffectPassDefinition& velocityPass) {
		return CreateGraphicsPipeline(sourceDevice, sourceSwapChain, depthFormat, passes.model,
			VK_CULL_MODE_BACK_BIT, false, false, false, false, VK_COMPARE_OP_LESS,
			sourceSwapChain.imageFormat, sourceDevice.msaaSampleCount, false, false, pipeline)
			&& CreateGraphicsPipeline(sourceDevice, sourceSwapChain, depthFormat, passes.model,
			VK_CULL_MODE_NONE, false, false, false, false, VK_COMPARE_OP_LESS,
			sourceSwapChain.imageFormat, sourceDevice.msaaSampleCount, false, false, bothFacePipeline)
			&& CreateDepthOnlyPipeline(sourceDevice, sourceSwapChain, depthFormat, depthPass,
			VK_CULL_MODE_BACK_BIT, depthOnlyPipeline)
			&& CreateDepthOnlyPipeline(sourceDevice, sourceSwapChain, depthFormat, depthPass,
			VK_CULL_MODE_NONE, depthOnlyBothFacePipeline)
			&& CreateGraphicsPipeline(sourceDevice, sourceSwapChain, depthFormat, velocityPass,
			VK_CULL_MODE_BACK_BIT, false, false, false, false, VK_COMPARE_OP_LESS,
			VK_FORMAT_R16G16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true, false, sceneVelocityPipeline)
			&& CreateGraphicsPipeline(sourceDevice, sourceSwapChain, depthFormat, velocityPass,
			VK_CULL_MODE_NONE, false, false, false, false, VK_COMPARE_OP_LESS,
			VK_FORMAT_R16G16_SFLOAT, VK_SAMPLE_COUNT_1_BIT, true, false, sceneVelocityBothFacePipeline)
			&& CreateGraphicsPipeline(sourceDevice, sourceSwapChain, depthFormat, passes.edge,
			VK_CULL_MODE_FRONT_BIT, false, false, false, false, VK_COMPARE_OP_LESS,
			sourceSwapChain.imageFormat, sourceDevice.msaaSampleCount, false, false, edgePipeline)
			&& CreateGraphicsPipeline(sourceDevice, sourceSwapChain, depthFormat, passes.groundShadow,
			VK_CULL_MODE_NONE, true, true, true, true, VK_COMPARE_OP_LESS,
			sourceSwapChain.imageFormat, sourceDevice.msaaSampleCount, false, true, groundShadowPipeline);
	}

	bool VulkanPipeline::CreateGraphicsPipeline(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, const EffectPassDefinition& pass,
		const VkCullModeFlags cullMode, const bool usePositionOnly, const bool useDepthBias, const bool enableStencilTest, const bool disableDepthWrite,
		const VkCompareOp depthCompareOp, const VkFormat colorFormat, const VkSampleCountFlagBits sampleCount,
		const bool useVelocityInput, const bool preserveDestinationAlpha, VkPipeline& outPipeline) const {
		std::string error;
		VulkanShaderStageBuilder shaderStages;
		if (!shaderStages.Build(sourceDevice, pass, SpirvBindingProfile::Scene, error)) {
			if (!error.empty())
				std::cerr << error << '\n';
			return false;
		}
		const VkVertexInputBindingDescription bindingDescription = MakeVertexBindingDescription();
		VkVertexInputAttributeDescription attributeDescriptions[3]{};
		FillVertexAttributeDescriptions(attributeDescriptions);
		if (useVelocityInput) {
			attributeDescriptions[1] = {
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(ViewerVertex, previousPosition)
			};
		}
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = usePositionOnly ? 1 : 3;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = sourceSwapChain.extent.width;
		viewport.height = sourceSwapChain.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = sourceSwapChain.extent;
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = cullMode;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = useDepthBias ? VK_TRUE : VK_FALSE;
		rasterizer.depthBiasConstantFactor = useDepthBias ? -1.0f : 0.0f;
		rasterizer.depthBiasClamp = 0.0f;
		rasterizer.depthBiasSlopeFactor = useDepthBias ? -1.0f : 0.0f;
		rasterizer.lineWidth = 1.0f;
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = sampleCount;
		multisampling.sampleShadingEnable = VK_FALSE;
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = disableDepthWrite ? VK_FALSE : VK_TRUE;
		depthStencil.depthCompareOp = depthCompareOp;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = enableStencilTest ? VK_TRUE : VK_FALSE;
		if (enableStencilTest) {
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
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.blendEnable = useVelocityInput ? VK_FALSE : VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = preserveDestinationAlpha
			? VK_BLEND_FACTOR_ZERO : VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor = preserveDestinationAlpha
			? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT |
			VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		const bool depthHasStencil = depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
			depthFormat == VK_FORMAT_D24_UNORM_S8_UINT;
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &colorFormat,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = depthHasStencil ? depthFormat : VK_FORMAT_UNDEFINED
		};
		createInfo.pNext = &renderingInfo;
		createInfo.stageCount = VulkanShaderStageBuilder::stageCount;
		createInfo.pStages = shaderStages.GetStages();
		createInfo.pVertexInputState = &vertexInputInfo;
		createInfo.pInputAssemblyState = &inputAssembly;
		createInfo.pViewportState = &viewportState;
		createInfo.pRasterizationState = &rasterizer;
		createInfo.pMultisampleState = &multisampling;
		createInfo.pDepthStencilState = &depthStencil;
		createInfo.pColorBlendState = &colorBlending;
		createInfo.layout = pipelineLayout;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &outPipeline) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan graphics pipeline.\n";
			return false;
		}
		return true;
	}

	bool VulkanPipeline::CreateDepthOnlyPipeline(
		const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, const EffectPassDefinition& pass,
		const VkCullModeFlags cullMode, VkPipeline& outPipeline) const {
		std::string error;
		VulkanShaderStageBuilder shaderStages;
		if (!shaderStages.Build(sourceDevice, pass, SpirvBindingProfile::Scene, error)) {
			if (!error.empty())
				std::cerr << error << '\n';
			return false;
		}
		const VkVertexInputBindingDescription bindingDescription = MakeVertexBindingDescription();
		constexpr VkVertexInputAttributeDescription attributeDescriptions[] = {
			VkVertexInputAttributeDescription{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = offsetof(ViewerVertex, position)
			},
			VkVertexInputAttributeDescription{
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(ViewerVertex, uv)
			}
		};
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = std::size(attributeDescriptions);
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		const VkViewport viewport{
			.x = 0.0f, .y = 0.0f,
			.width = static_cast<float>(sourceSwapChain.extent.width),
			.height = static_cast<float>(sourceSwapChain.extent.height),
			.minDepth = 0.0f, .maxDepth = 1.0f
		};
		const VkRect2D scissor{ .extent = sourceSwapChain.extent };
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = cullMode;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.0f;
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		VkGraphicsPipelineCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		const VkPipelineRenderingCreateInfo renderingInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 0,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
		};
		createInfo.pNext = &renderingInfo;
		createInfo.stageCount = VulkanShaderStageBuilder::stageCount;
		createInfo.pStages = shaderStages.GetStages();
		createInfo.pVertexInputState = &vertexInputInfo;
		createInfo.pInputAssemblyState = &inputAssembly;
		createInfo.pViewportState = &viewportState;
		createInfo.pRasterizationState = &rasterizer;
		createInfo.pMultisampleState = &multisampling;
		createInfo.pDepthStencilState = &depthStencil;
		createInfo.layout = pipelineLayout;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &outPipeline) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan depth-only pipeline.\n";
			return false;
		}
		return true;
	}

	VkVertexInputBindingDescription VulkanPipeline::MakeVertexBindingDescription() {
		VkVertexInputBindingDescription bindingDescription;
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(ViewerVertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	void VulkanPipeline::FillVertexAttributeDescriptions(VkVertexInputAttributeDescription (&descriptions)[3]) {
		descriptions[0] = {
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(ViewerVertex, position)
		};
		descriptions[1] = {
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(ViewerVertex, normal)
		};
		descriptions[2] = {
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = offsetof(ViewerVertex, uv)
		};
	}

	VulkanPipeline::~VulkanPipeline() {
		Reset();
	}

	VkPipeline VulkanPipeline::ResolveSceneInputPipeline(const bool velocity, const bool bothFace) const {
		if (velocity)
			return bothFace ? sceneVelocityBothFacePipeline : sceneVelocityPipeline;
		return bothFace ? depthOnlyBothFacePipeline : depthOnlyPipeline;
	}

	bool VulkanPipeline::Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
		const VkFormat depthFormat, const BuiltInShaderPasses& passes,
		const EffectPassDefinition& depthPass, const EffectPassDefinition& velocityPass) {
		device = sourceDevice.device;
		if (!CreateDescriptorSetLayouts())
			return false;
		if (!CreatePipelineLayout())
			return false;
		return CreateGraphicsPipelines(sourceDevice, sourceSwapChain, depthFormat,
			passes, depthPass, velocityPass);
	}

	void VulkanPipeline::Reset() {
		if (device == VK_NULL_HANDLE)
			return;
		if (pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, pipeline, nullptr);
			pipeline = VK_NULL_HANDLE;
		}
		if (bothFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, bothFacePipeline, nullptr);
			bothFacePipeline = VK_NULL_HANDLE;
		}
		if (depthOnlyPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, depthOnlyPipeline, nullptr);
			depthOnlyPipeline = VK_NULL_HANDLE;
		}
		if (depthOnlyBothFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, depthOnlyBothFacePipeline, nullptr);
			depthOnlyBothFacePipeline = VK_NULL_HANDLE;
		}
		if (sceneVelocityPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, sceneVelocityPipeline, nullptr);
			sceneVelocityPipeline = VK_NULL_HANDLE;
		}
		if (sceneVelocityBothFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, sceneVelocityBothFacePipeline, nullptr);
			sceneVelocityBothFacePipeline = VK_NULL_HANDLE;
		}
		if (edgePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, edgePipeline, nullptr);
			edgePipeline = VK_NULL_HANDLE;
		}
		if (groundShadowPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, groundShadowPipeline, nullptr);
			groundShadowPipeline = VK_NULL_HANDLE;
		}
		if (pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			pipelineLayout = VK_NULL_HANDLE;
		}
		for (VkDescriptorSetLayout& descriptorSetLayout : descriptorSetLayouts) {
			if (descriptorSetLayout != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
				descriptorSetLayout = VK_NULL_HANDLE;
			}
		}
		device = VK_NULL_HANDLE;
	}
}
