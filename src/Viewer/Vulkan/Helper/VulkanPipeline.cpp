#include "VulkanPipeline.h"

#include "VulkanShaderModule.h"

#include <iostream>

namespace Chrivent {
	VulkanPipeline::~VulkanPipeline() {
		Destroy();
	}

	bool VulkanPipeline::Initialize(
		const VulkanDeviceInfo& deviceInfo,
		const VulkanSwapChainInfo& swapChainInfo,
		const VkRenderPass renderPass,
		const std::filesystem::path& shaderDir) {
		device = deviceInfo.device;
		if (!CreateDescriptorSetLayouts())
			return false;
		if (!CreatePipelineLayout())
			return false;
		return CreateGraphicsPipeline(deviceInfo, swapChainInfo, renderPass, shaderDir);
	}

	void VulkanPipeline::Destroy() {
		if (device == VK_NULL_HANDLE)
			return;
		if (info.pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, info.pipeline, nullptr);
			info.pipeline = VK_NULL_HANDLE;
		}
		if (info.pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(device, info.pipelineLayout, nullptr);
			info.pipelineLayout = VK_NULL_HANDLE;
		}
		for (VkDescriptorSetLayout& descriptorSetLayout : info.descriptorSetLayouts) {
			if (descriptorSetLayout != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
				descriptorSetLayout = VK_NULL_HANDLE;
			}
		}
		device = VK_NULL_HANDLE;
	}

	bool VulkanPipeline::CreateDescriptorSetLayouts() {
		constexpr VkDescriptorSetLayoutBinding vertexConstantBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = nullptr
		};
		VkDescriptorSetLayoutCreateInfo vertexLayoutInfo{};
		vertexLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		vertexLayoutInfo.bindingCount = 1;
		vertexLayoutInfo.pBindings = &vertexConstantBinding;
		if (vkCreateDescriptorSetLayout(device, &vertexLayoutInfo, nullptr, &info.descriptorSetLayouts[0]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan vertex descriptor set layout.\n";
			return false;
		}
		constexpr VkDescriptorSetLayoutBinding pixelConstantBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};
		VkDescriptorSetLayoutCreateInfo pixelLayoutInfo{};
		pixelLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		pixelLayoutInfo.bindingCount = 1;
		pixelLayoutInfo.pBindings = &pixelConstantBinding;
		if (vkCreateDescriptorSetLayout(device, &pixelLayoutInfo, nullptr, &info.descriptorSetLayouts[1]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan pixel descriptor set layout.\n";
			return false;
		}
		constexpr std::array textureBindings = {
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			},
			VkDescriptorSetLayoutBinding{
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			},
			VkDescriptorSetLayoutBinding{
				.binding = 2,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = nullptr
			}
		};
		VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
		textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		textureLayoutInfo.bindingCount = textureBindings.size();
		textureLayoutInfo.pBindings = textureBindings.data();
		if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr, &info.descriptorSetLayouts[2]) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan texture descriptor set layout.\n";
			return false;
		}
		return true;
	}

	bool VulkanPipeline::CreatePipelineLayout() {
		VkPipelineLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		createInfo.setLayoutCount = info.descriptorSetLayouts.size();
		createInfo.pSetLayouts = info.descriptorSetLayouts.data();
		if (vkCreatePipelineLayout(device, &createInfo, nullptr, &info.pipelineLayout) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan pipeline layout.\n";
			return false;
		}
		return true;
	}

	bool VulkanPipeline::CreateGraphicsPipeline(
		const VulkanDeviceInfo& deviceInfo,
		const VulkanSwapChainInfo& swapChainInfo,
		const VkRenderPass renderPass,
		const std::filesystem::path& shaderDir) {
		VulkanShaderModule vertexShader;
		VulkanShaderModule fragmentShader;
		if (!vertexShader.Initialize(deviceInfo, shaderDir / "model.vert.spv"))
			return false;
		if (!fragmentShader.Initialize(deviceInfo, shaderDir / "model.frag.spv"))
			return false;
		const VkPipelineShaderStageCreateInfo shaderStages[] = {
			MakeShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader.GetShaderModule()),
			MakeShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader.GetShaderModule())
		};
		const VkVertexInputBindingDescription vertexBinding = MakeVertexBindingDescription();
		const auto vertexAttributes = MakeVertexAttributeDescriptions();
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &vertexBinding;
		vertexInputInfo.vertexAttributeDescriptionCount = vertexAttributes.size();
		vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = swapChainInfo.extent.width;
		viewport.height = swapChainInfo.extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainInfo.extent;
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
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.lineWidth = 1.0f;
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.sampleShadingEnable = VK_FALSE;
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
		createInfo.stageCount = 2;
		createInfo.pStages = shaderStages;
		createInfo.pVertexInputState = &vertexInputInfo;
		createInfo.pInputAssemblyState = &inputAssembly;
		createInfo.pViewportState = &viewportState;
		createInfo.pRasterizationState = &rasterizer;
		createInfo.pMultisampleState = &multisampling;
		createInfo.pDepthStencilState = &depthStencil;
		createInfo.pColorBlendState = &colorBlending;
		createInfo.layout = info.pipelineLayout;
		createInfo.renderPass = renderPass;
		createInfo.subpass = 0;
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &info.pipeline) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan graphics pipeline.\n";
			return false;
		}
		return true;
	}

	VkPipelineShaderStageCreateInfo VulkanPipeline::MakeShaderStageInfo(const VkShaderStageFlagBits stage, const VkShaderModule shaderModule) {
		VkPipelineShaderStageCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		createInfo.stage = stage;
		createInfo.module = shaderModule;
		createInfo.pName = "main";
		return createInfo;
	}

	VkVertexInputBindingDescription VulkanPipeline::MakeVertexBindingDescription() {
		VkVertexInputBindingDescription bindingDescription;
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(float) * 8;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescription;
	}

	std::array<VkVertexInputAttributeDescription, 3> VulkanPipeline::MakeVertexAttributeDescriptions() {
		return {
			VkVertexInputAttributeDescription{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = 0
			},
			VkVertexInputAttributeDescription{
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32_SFLOAT,
				.offset = sizeof(float) * 3
			},
			VkVertexInputAttributeDescription{
				.location = 2,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = sizeof(float) * 6
			}
		};
	}
}
