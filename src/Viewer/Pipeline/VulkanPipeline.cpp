#include "Viewer/Pipeline/VulkanPipeline.h"

#include "Viewer/Pipeline/VulkanGraphicsPipelineBuilder.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <iterator>

namespace Chrivent {
	GraphicsResult<void> VulkanPipeline::CreateDescriptorSetLayouts() {
		static constexpr VkDescriptorSetLayoutBinding vertexConstantBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
		};
		static constexpr VkDescriptorSetLayoutCreateInfo vertexLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &vertexConstantBinding
		};
		VkResult result = vkCreateDescriptorSetLayout(device, &vertexLayoutInfo,
			nullptr, &descriptorSetLayouts[0]);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "vertex descriptor set layout 생성",
				"Vulkan vertex descriptor set layout을 만들지 못했습니다", result, true));
		}
		static constexpr VkDescriptorSetLayoutBinding pixelConstantBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		static constexpr VkDescriptorSetLayoutCreateInfo pixelLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &pixelConstantBinding
		};
		result = vkCreateDescriptorSetLayout(device, &pixelLayoutInfo,
			nullptr, &descriptorSetLayouts[1]);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "pixel descriptor set layout 생성",
				"Vulkan pixel descriptor set layout을 만들지 못했습니다", result, true));
		}
		static constexpr VkDescriptorSetLayoutBinding textureBindings[] = {
			VkDescriptorSetLayoutBinding{
				.binding = SceneShaderInputLayout::baseTextureRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = SceneShaderInputLayout::toonTextureRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = SceneShaderInputLayout::sphereTextureRegister,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::baseSamplerRegister),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::toonSamplerRegister),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
			VkDescriptorSetLayoutBinding{
				.binding = SpirvBindingLayout::ResolveSamplerBinding(SceneShaderInputLayout::sphereSamplerRegister),
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			}
		};
		static constexpr VkDescriptorSetLayoutCreateInfo textureLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = std::size(textureBindings),
			.pBindings = textureBindings
		};
		result = vkCreateDescriptorSetLayout(device, &textureLayoutInfo,
			nullptr, &descriptorSetLayouts[2]);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture descriptor set layout 생성",
				"Vulkan texture descriptor set layout을 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> VulkanPipeline::CreatePipelineLayout() {
		const VkPipelineLayoutCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 3,
			.pSetLayouts = descriptorSetLayouts
		};
		const VkResult result = vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "pipeline layout 생성",
				"Vulkan pipeline layout을 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> VulkanPipeline::CreateGraphicsPipelines(const VulkanDevice& sourceDevice,
		const VkFormat sourceColorFormat, const VkFormat sourceDepthFormat,
		const BuiltInShaderPasses& passes, const ShaderProgramDefinition& depthProgram,
		const ShaderProgramDefinition& velocityProgram) {
		using Builder = VulkanGraphicsPipelineBuilder;
		Builder::Configuration configuration{
			.pipelineLayout = pipelineLayout,
			.colorFormat = sourceColorFormat,
			.depthFormat = sourceDepthFormat,
			.sampleCount = sourceDevice.GetMsaaSampleCount()
		};
		auto result = Builder::Create(sourceDevice, passes.model, configuration, pipeline);
		if (!result)
			return result;
		configuration.cullMode = VK_CULL_MODE_NONE;
		result = Builder::Create(sourceDevice, passes.model, configuration, bothFacePipeline);
		if (!result)
			return result;
		configuration.colorFormat = VK_FORMAT_UNDEFINED;
		configuration.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		configuration.cullMode = VK_CULL_MODE_BACK_BIT;
		configuration.vertexLayout = Builder::VertexLayout::PositionUv;
		result = Builder::Create(sourceDevice, depthProgram, configuration, depthOnlyPipeline);
		if (!result)
			return result;
		configuration.cullMode = VK_CULL_MODE_NONE;
		result = Builder::Create(
			sourceDevice, depthProgram, configuration, depthOnlyBothFacePipeline);
		if (!result)
			return result;
		configuration.colorFormat = VK_FORMAT_R16G16_SFLOAT;
		configuration.cullMode = VK_CULL_MODE_BACK_BIT;
		configuration.vertexLayout = Builder::VertexLayout::Velocity;
		result = Builder::Create(
			sourceDevice, velocityProgram, configuration, sceneVelocityPipeline);
		if (!result)
			return result;
		configuration.cullMode = VK_CULL_MODE_NONE;
		result = Builder::Create(
			sourceDevice, velocityProgram, configuration, sceneVelocityBothFacePipeline);
		if (!result)
			return result;
		configuration.colorFormat = sourceColorFormat;
		configuration.sampleCount = sourceDevice.GetMsaaSampleCount();
		configuration.cullMode = VK_CULL_MODE_FRONT_BIT;
		configuration.vertexLayout = Builder::VertexLayout::Model;
		result = Builder::Create(sourceDevice, passes.edge, configuration, edgePipeline);
		if (!result)
			return result;
		configuration.cullMode = VK_CULL_MODE_NONE;
		configuration.vertexLayout = Builder::VertexLayout::PositionOnly;
		configuration.depthBiasEnabled = true;
		configuration.stencilTestEnabled = true;
		configuration.depthWriteDisabled = true;
		configuration.preserveDestinationAlpha = true;
		return Builder::Create(
			sourceDevice, passes.groundShadow, configuration, groundShadowPipeline);
	}

	VulkanPipeline::~VulkanPipeline() {
		Reset();
	}

	VkPipeline VulkanPipeline::ResolveSceneInputPipeline(const bool velocity,
		const bool bothFace) const {
		if (velocity)
			return bothFace ? sceneVelocityBothFacePipeline : sceneVelocityPipeline;
		return bothFace ? depthOnlyBothFacePipeline : depthOnlyPipeline;
	}

	GraphicsResult<void> VulkanPipeline::Initialize(const VulkanDevice& sourceDevice,
		const VkFormat sourceColorFormat, const VkFormat sourceDepthFormat,
		const SceneShaderRuntimeContract& shaderContract) {
		Reset();
		device = sourceDevice.GetDevice();
		const auto descriptorLayoutResult = CreateDescriptorSetLayouts();
		if (!descriptorLayoutResult)
			return std::unexpected(descriptorLayoutResult.error());
		const auto pipelineLayoutResult = CreatePipelineLayout();
		if (!pipelineLayoutResult)
			return std::unexpected(pipelineLayoutResult.error());
		const auto graphicsPipelineResult = CreateGraphicsPipelines(
			sourceDevice, sourceColorFormat, sourceDepthFormat,
			shaderContract.builtIn, shaderContract.sceneInput.depth,
			shaderContract.sceneInput.velocity);
		if (!graphicsPipelineResult)
			return std::unexpected(graphicsPipelineResult.error());
		colorFormat = sourceColorFormat;
		depthFormat = sourceDepthFormat;
		sampleCount = sourceDevice.GetMsaaSampleCount();
		return {};
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
		colorFormat = VK_FORMAT_UNDEFINED;
		depthFormat = VK_FORMAT_UNDEFINED;
		sampleCount = VK_SAMPLE_COUNT_1_BIT;
		device = VK_NULL_HANDLE;
	}
}
