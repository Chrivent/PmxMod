#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/ShaderProgramDefinition.h"

#include <span>
#include <vector>

namespace Chrivent {
	// Vulkan 후처리 패스별 동적 viewport 그래픽 파이프라인을 생성하고 소유한다.
	class VulkanPostProcessPipelines {
		VkDevice device = VK_NULL_HANDLE;
		std::vector<VkPipeline> pipelines;
		std::vector<VkFormat> targetFormats;

		// HLSL 후처리 패스 하나를 지정한 출력 형식의 파이프라인으로 만든다.
		GraphicsResult<void> CreateGraphicsPipeline(
			const VulkanDevice& sourceDevice, VkPipelineLayout pipelineLayout,
			const ShaderProgramDefinition& program, VkFormat targetFormat, VkPipeline& pipeline) const;

	public:
		VulkanPostProcessPipelines() = default;
		~VulkanPostProcessPipelines();
		
		VulkanPostProcessPipelines(const VulkanPostProcessPipelines&) = delete;
		VulkanPostProcessPipelines& operator=(const VulkanPostProcessPipelines&) = delete;

		VkPipeline TryGetPipeline(const size_t index) const {
			return index < pipelines.size() ? pipelines[index] : VK_NULL_HANDLE;
		}
		size_t GetCount() const { return pipelines.size(); }

		// 현재 파이프라인들이 지정한 출력 형식 목록과 호환되는지 확인한다.
		bool IsCompatible(std::span<const VkFormat> formats) const;
		// 패스와 출력 정보 목록을 검증한 뒤 모든 Vulkan 후처리 파이프라인을 생성한다.
		GraphicsResult<void> Initialize(
			const VulkanDevice& sourceDevice, VkPipelineLayout pipelineLayout,
			std::span<const ShaderProgramDefinition> programs,
			std::span<const VkFormat> formats);
		// 다른 파이프라인 묶음과 소유권을 교환한다.
		void Swap(VulkanPostProcessPipelines& other) noexcept;
		// 생성한 Vulkan 파이프라인을 해제한다.
		void Reset();
	};
}
