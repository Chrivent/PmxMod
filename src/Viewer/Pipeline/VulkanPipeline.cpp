#include "Viewer/Pipeline/VulkanPipeline.h"

#include "Viewer/Pipeline/VulkanPipelineBuilder.h"
#include "Viewer/Shader/SceneShaderInputLayout.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <iterator>

namespace Chrivent {
	GraphicsError::Result<void> VulkanPipeline::CreateDescriptorSetLayouts() {
		static constexpr VkDescriptorSetLayoutBinding vertexConstantBinding{
			.binding = SpirvBindingLayout::frameDataBinding,
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
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "vertex descriptor set layout 생성",
				"Vulkan vertex descriptor set layout을 만들지 못했습니다", result, true));
		}
		static constexpr VkDescriptorSetLayoutBinding pixelConstantBinding{
			.binding = SpirvBindingLayout::parameterDataBinding,
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
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
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
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "texture descriptor set layout 생성",
				"Vulkan texture descriptor set layout을 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanPipeline::CreatePipelineLayout() {
		const VkPipelineLayoutCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 3,
			.pSetLayouts = descriptorSetLayouts
		};
		const VkResult result = vkCreatePipelineLayout(device, &createInfo, nullptr, &pipelineLayout);
		if (result != VK_SUCCESS) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "pipeline layout 생성",
				"Vulkan pipeline layout을 만들지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsError::Result<void> VulkanPipeline::CreateGraphicsPipelines(const VulkanDevice& sourceDevice,
		const VkFormat sourceColorFormat, const VkFormat sourceDepthFormat,
		const BuiltInShaderPasses& passes, const ShaderProgramDefinition& depthProgram,
		const ShaderProgramDefinition& velocityProgram) {
		using Builder = VulkanPipelineBuilder;
		Builder::Configuration configuration{
			.pipelineLayout = pipelineLayout,
			.colorFormat = sourceColorFormat,
			.depthFormat = sourceDepthFormat,
			.sampleCount = sourceDevice.GetMsaaSampleCount()
		};
		auto result = Builder::Create(
			sourceDevice, passes.model, configuration, modelFrontFacePipeline);
		if (!result)
			return result;
		configuration.cullMode = VK_CULL_MODE_NONE;
		result = Builder::Create(
			sourceDevice, passes.model, configuration, modelBothFacePipeline);
		if (!result)
			return result;
		configuration.colorFormat = VK_FORMAT_UNDEFINED;
		configuration.sampleCount = VK_SAMPLE_COUNT_1_BIT;
		configuration.cullMode = VK_CULL_MODE_BACK_BIT;
		configuration.vertexLayout = Builder::VertexLayout::PositionUv;
		result = Builder::Create(
			sourceDevice, depthProgram, configuration, sceneDepthFrontFacePipeline);
		if (!result)
			return result;
		configuration.cullMode = VK_CULL_MODE_NONE;
		result = Builder::Create(
			sourceDevice, depthProgram, configuration, sceneDepthBothFacePipeline);
		if (!result)
			return result;
		configuration.colorFormat = VK_FORMAT_R16G16_SFLOAT;
		configuration.cullMode = VK_CULL_MODE_BACK_BIT;
		configuration.vertexLayout = Builder::VertexLayout::Velocity;
		result = Builder::Create(
			sourceDevice, velocityProgram, configuration, sceneVelocityFrontFacePipeline);
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

	bool VulkanPipeline::IsCompatible(const VkFormat sourceColorFormat,
		const VkFormat sourceDepthFormat, const VkSampleCountFlagBits sourceSampleCount) const {
		return modelFrontFacePipeline != VK_NULL_HANDLE && colorFormat == sourceColorFormat
			&& depthFormat == sourceDepthFormat && sampleCount == sourceSampleCount;
	}

	void VulkanPipeline::SwapResources(VulkanPipeline& other) noexcept {
		std::swap(device, other.device);
		std::swap(modelFrontFacePipeline, other.modelFrontFacePipeline);
		std::swap(modelBothFacePipeline, other.modelBothFacePipeline);
		std::swap(sceneDepthFrontFacePipeline, other.sceneDepthFrontFacePipeline);
		std::swap(sceneDepthBothFacePipeline, other.sceneDepthBothFacePipeline);
		std::swap(sceneVelocityFrontFacePipeline, other.sceneVelocityFrontFacePipeline);
		std::swap(sceneVelocityBothFacePipeline, other.sceneVelocityBothFacePipeline);
		std::swap(edgePipeline, other.edgePipeline);
		std::swap(groundShadowPipeline, other.groundShadowPipeline);
		std::swap(pipelineLayout, other.pipelineLayout);
		for (size_t index = 0; index < std::size(descriptorSetLayouts); index++)
			std::swap(descriptorSetLayouts[index], other.descriptorSetLayouts[index]);
		std::swap(colorFormat, other.colorFormat);
		std::swap(depthFormat, other.depthFormat);
		std::swap(sampleCount, other.sampleCount);
		std::swap(shaderContract, other.shaderContract);
	}

	GraphicsError::Result<void> VulkanPipeline::Initialize(const VulkanDevice& sourceDevice,
		const VkFormat sourceColorFormat, const VkFormat sourceDepthFormat,
		const SceneShaderRuntimeContract& sourceShaderContract) {
		if (sourceDevice.GetDevice() == VK_NULL_HANDLE) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "graphics pipeline 초기화",
				"Vulkan device를 사용할 수 없습니다"));
		}
		VulkanPipeline candidate;
		candidate.device = sourceDevice.GetDevice();
		const auto descriptorLayoutResult = candidate.CreateDescriptorSetLayouts();
		if (!descriptorLayoutResult)
			return std::unexpected(descriptorLayoutResult.error());
		const auto pipelineLayoutResult = candidate.CreatePipelineLayout();
		if (!pipelineLayoutResult)
			return std::unexpected(pipelineLayoutResult.error());
		const auto graphicsPipelineResult = candidate.CreateGraphicsPipelines(
			sourceDevice, sourceColorFormat, sourceDepthFormat,
			sourceShaderContract.builtIn, sourceShaderContract.sceneInput.depth,
			sourceShaderContract.sceneInput.velocity);
		if (!graphicsPipelineResult)
			return std::unexpected(graphicsPipelineResult.error());
		candidate.colorFormat = sourceColorFormat;
		candidate.depthFormat = sourceDepthFormat;
		candidate.sampleCount = sourceDevice.GetMsaaSampleCount();
		candidate.shaderContract = sourceShaderContract;
		SwapResources(candidate);
		return {};
	}

	GraphicsError::Result<void> VulkanPipeline::RecreateIfIncompatible(const VulkanDevice& sourceDevice,
		const VkFormat sourceColorFormat, const VkFormat sourceDepthFormat) {
		if (IsCompatible(sourceColorFormat, sourceDepthFormat, sourceDevice.GetMsaaSampleCount()))
			return {};
		if (shaderContract.builtIn.model.shaderPath.empty()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "graphics pipeline 재생성",
				"Vulkan 장면 셰이더 계약이 저장되지 않았습니다"));
		}
		return Initialize(sourceDevice, sourceColorFormat, sourceDepthFormat, shaderContract);
	}

	void VulkanPipeline::Reset() {
		if (device == VK_NULL_HANDLE)
			return;
		if (modelFrontFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, modelFrontFacePipeline, nullptr);
			modelFrontFacePipeline = VK_NULL_HANDLE;
		}
		if (modelBothFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, modelBothFacePipeline, nullptr);
			modelBothFacePipeline = VK_NULL_HANDLE;
		}
		if (sceneDepthFrontFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, sceneDepthFrontFacePipeline, nullptr);
			sceneDepthFrontFacePipeline = VK_NULL_HANDLE;
		}
		if (sceneDepthBothFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, sceneDepthBothFacePipeline, nullptr);
			sceneDepthBothFacePipeline = VK_NULL_HANDLE;
		}
		if (sceneVelocityFrontFacePipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(device, sceneVelocityFrontFacePipeline, nullptr);
			sceneVelocityFrontFacePipeline = VK_NULL_HANDLE;
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
		shaderContract = {};
		device = VK_NULL_HANDLE;
	}
}
