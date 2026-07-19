#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/SceneShaderRuntimeContract.h"

namespace Chrivent {
	// Vulkan 모델 렌더링에 필요한 descriptor layout과 그래픽 파이프라인을 관리한다.
	class VulkanPipeline {
		VkDevice device = VK_NULL_HANDLE;
		VkPipeline modelFrontFacePipeline = VK_NULL_HANDLE;
		VkPipeline modelBothFacePipeline = VK_NULL_HANDLE;
		VkPipeline sceneDepthFrontFacePipeline = VK_NULL_HANDLE;
		VkPipeline sceneDepthBothFacePipeline = VK_NULL_HANDLE;
		VkPipeline sceneVelocityFrontFacePipeline = VK_NULL_HANDLE;
		VkPipeline sceneVelocityBothFacePipeline = VK_NULL_HANDLE;
		VkPipeline edgePipeline = VK_NULL_HANDLE;
		VkPipeline groundShadowPipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayouts[3]{};
		VkFormat colorFormat = VK_FORMAT_UNDEFINED;
		VkFormat depthFormat = VK_FORMAT_UNDEFINED;
		VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
		SceneShaderRuntimeContract shaderContract;

		// 모델 데이터에서 사용할 descriptor set layout들을 생성한다.
		GraphicsError::Result<void> CreateDescriptorSetLayouts();
		// descriptor set layout들을 묶은 pipeline layout을 생성한다.
		GraphicsError::Result<void> CreatePipelineLayout();
		// 모델 렌더링용 graphics pipeline들을 생성한다.
		GraphicsError::Result<void> CreateGraphicsPipelines(
			const VulkanDevice& sourceDevice, VkFormat sourceColorFormat,
			VkFormat sourceDepthFormat, const BuiltInShaderPasses& passes,
			const ShaderProgramDefinition& depthProgram,
			const ShaderProgramDefinition& velocityProgram);
		// 현재 파이프라인이 지정한 attachment 형식 및 샘플 수와 호환되는지 반환한다.
		bool IsCompatible(VkFormat sourceColorFormat, VkFormat sourceDepthFormat,
			VkSampleCountFlagBits sourceSampleCount) const;
		// 검증된 다른 Vulkan 파이프라인과 소유 리소스를 교환한다.
		void SwapResources(VulkanPipeline& other) noexcept;

	public:
		VulkanPipeline() = default;
		~VulkanPipeline();

		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;

		// 재질 방향성에 맞는 모델 파이프라인을 반환한다.
		VkPipeline ResolveModelPipeline(const bool bothFace) const {
			return bothFace ? modelBothFacePipeline : modelFrontFacePipeline;
		}
		// 재질 방향성에 맞는 장면 depth 파이프라인을 반환한다.
		VkPipeline ResolveSceneDepthPipeline(const bool bothFace) const {
			return bothFace ? sceneDepthBothFacePipeline : sceneDepthFrontFacePipeline;
		}
		// 재질 방향성에 맞는 장면 velocity 파이프라인을 반환한다.
		VkPipeline ResolveSceneVelocityPipeline(const bool bothFace) const {
			return bothFace ? sceneVelocityBothFacePipeline : sceneVelocityFrontFacePipeline;
		}
		// 엣지 렌더링 파이프라인을 반환한다.
		VkPipeline GetEdgePipeline() const { return edgePipeline; }
		// 지면 그림자 렌더링 파이프라인을 반환한다.
		VkPipeline GetGroundShadowPipeline() const { return groundShadowPipeline; }
		// 모델 descriptor set들을 바인딩할 공통 pipeline layout을 반환한다.
		VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }
		// 모델 vertex 상수용 descriptor set layout을 반환한다.
		VkDescriptorSetLayout GetVertexDescriptorSetLayout() const { return descriptorSetLayouts[0]; }
		// 재질 pixel 상수용 descriptor set layout을 반환한다.
		VkDescriptorSetLayout GetPixelDescriptorSetLayout() const { return descriptorSetLayouts[1]; }
		// 재질 texture용 descriptor set layout을 반환한다.
		VkDescriptorSetLayout GetTextureDescriptorSetLayout() const { return descriptorSetLayouts[2]; }
		// 스왑체인 attachment format에 맞는 모델 graphics pipeline을 생성한다.
		GraphicsError::Result<void> Initialize(const VulkanDevice& sourceDevice, VkFormat sourceColorFormat,
			VkFormat sourceDepthFormat, const SceneShaderRuntimeContract& shaderContract);
		// 저장된 셰이더 계약으로 호환되지 않는 Vulkan graphics pipeline만 다시 생성한다.
		GraphicsError::Result<void> RecreateIfIncompatible(const VulkanDevice& sourceDevice,
			VkFormat sourceColorFormat, VkFormat sourceDepthFormat);
		// 생성한 pipeline 리소스를 해제한다.
		void Reset();
	};
}
