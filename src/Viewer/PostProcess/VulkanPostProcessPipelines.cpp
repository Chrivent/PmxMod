#include "Viewer/PostProcess/VulkanPostProcessPipelines.h"

#include "Viewer/Pipeline/VulkanPipelineBuilder.h"

#include <algorithm>

namespace Chrivent {
	GraphicsError::Result<void> VulkanPostProcessPipelines::CreateGraphicsPipeline(
		const VulkanDevice& sourceDevice,
		const VkPipelineLayout pipelineLayout, const ShaderProgramDefinition& program,
		const VkFormat targetFormat, VkPipeline& pipeline) {
		const VulkanPipelineBuilder::Configuration configuration{
			.pipelineLayout = pipelineLayout,
			.colorFormat = targetFormat,
			.cullMode = VK_CULL_MODE_NONE,
			.vertexLayout = VulkanPipelineBuilder::VertexLayout::None,
			.bindingProfile = SpirvBindingProfile::PostProcess,
			.invertVertexY = true,
			.blendEnabled = false
		};
		return VulkanPipelineBuilder::Create(sourceDevice, program, configuration, pipeline);
	}

	VulkanPostProcessPipelines::~VulkanPostProcessPipelines() {
		Reset();
	}

	bool VulkanPostProcessPipelines::IsCompatible(const std::span<const VkFormat> formats) const {
		return pipelines.size() == formats.size() && std::ranges::equal(targetFormats, formats);
	}

	GraphicsError::Result<void> VulkanPostProcessPipelines::Initialize(
		const VulkanDevice& sourceDevice,
		const VkPipelineLayout pipelineLayout, const std::span<const ShaderProgramDefinition> programs,
		const std::span<const VkFormat> formats) {
		Reset();
		device = sourceDevice.GetDevice();
		if (programs.empty())
			return {};
		if (device == VK_NULL_HANDLE || pipelineLayout == VK_NULL_HANDLE) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "후처리 pipeline 초기화",
				"Vulkan device 또는 후처리 pipeline layout을 사용할 수 없습니다"));
		}
		if (programs.size() != formats.size()) {
			return std::unexpected(GraphicsError::Create(GraphicsApi::Vulkan,
				GraphicsErrorCode::ContractViolation, "후처리 pipeline 초기화",
				"후처리 프로그램과 출력 형식 개수가 일치하지 않습니다"));
		}
		for (size_t index = 0; index < programs.size(); index++) {
			VkPipeline pipeline = VK_NULL_HANDLE;
			const auto result = CreateGraphicsPipeline(sourceDevice, pipelineLayout,
				programs[index], formats[index], pipeline);
			if (!result) {
				const GraphicsError error = result.error();
				Reset();
				return std::unexpected(error);
			}
			pipelines.emplace_back(pipeline);
		}
		targetFormats.assign(formats.begin(), formats.end());
		return {};
	}

	void VulkanPostProcessPipelines::Swap(VulkanPostProcessPipelines& other) noexcept {
		std::swap(device, other.device);
		pipelines.swap(other.pipelines);
		targetFormats.swap(other.targetFormats);
	}

	void VulkanPostProcessPipelines::Reset() {
		if (device != VK_NULL_HANDLE) {
			for (const VkPipeline pipeline : pipelines) {
				if (pipeline != VK_NULL_HANDLE)
					vkDestroyPipeline(device, pipeline, nullptr);
			}
		}
		pipelines.clear();
		targetFormats.clear();
		device = VK_NULL_HANDLE;
	}
}
