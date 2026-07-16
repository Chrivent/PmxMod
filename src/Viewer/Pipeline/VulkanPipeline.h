#pragma once

#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/SwapChain/VulkanSwapChain.h"
#include "Viewer/Shader/ShaderRuntimeContract.h"

namespace Chrivent {
	// Vulkan 모델 렌더링에 필요한 descriptor layout과 그래픽 파이프라인을 관리한다.
	class VulkanPipeline {
		VkDevice device = VK_NULL_HANDLE;
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipeline bothFacePipeline = VK_NULL_HANDLE;
		VkPipeline depthOnlyPipeline = VK_NULL_HANDLE;
		VkPipeline depthOnlyBothFacePipeline = VK_NULL_HANDLE;
		VkPipeline sceneVelocityPipeline = VK_NULL_HANDLE;
		VkPipeline sceneVelocityBothFacePipeline = VK_NULL_HANDLE;
		VkPipeline edgePipeline = VK_NULL_HANDLE;
		VkPipeline groundShadowPipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayouts[3]{};

		// 모델 데이터에서 사용할 descriptor set layout들을 생성한다.
		bool CreateDescriptorSetLayouts();
		// descriptor set layout들을 묶은 pipeline layout을 생성한다.
		bool CreatePipelineLayout();
		// 모델 렌더링용 graphics pipeline들을 생성한다.
		bool CreateGraphicsPipelines(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkFormat depthFormat, const BuiltInShaderPasses& passes,
			const EffectPassDefinition& depthPass, const EffectPassDefinition& velocityPass);
		// 지정한 cull mode로 모델 렌더링용 graphics pipeline을 생성한다.
		bool CreateGraphicsPipeline(
			const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkFormat depthFormat, const EffectPassDefinition& pass,
			VkCullModeFlags cullMode, bool usePositionOnly, bool useDepthBias, bool enableStencilTest, bool disableDepthWrite,
			VkCompareOp depthCompareOp, VkFormat colorFormat, VkSampleCountFlagBits sampleCount,
			bool useVelocityInput, VkPipeline& outPipeline) const;
		// 지정한 cull mode로 후처리 depth-only graphics pipeline을 생성한다.
		bool CreateDepthOnlyPipeline(
			const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkFormat depthFormat, const EffectPassDefinition& pass,
			VkCullModeFlags cullMode, VkPipeline& outPipeline) const;
		// 모델 vertex buffer binding 정보를 만든다.
		static VkVertexInputBindingDescription MakeVertexBindingDescription();
		// 모델 vertex attribute 정보를 채운다.
		static void FillVertexAttributeDescriptions(VkVertexInputAttributeDescription (&descriptions)[3]);

	public:
		VulkanPipeline() = default;
		~VulkanPipeline();

		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;

		// 재질 방향성에 맞는 모델 파이프라인을 반환한다.
		VkPipeline ResolveModelPipeline(const bool bothFace) const {
			return bothFace ? bothFacePipeline : pipeline;
		}
		// 입력 종류와 재질 방향성에 맞는 장면 입력 파이프라인을 반환한다.
		VkPipeline ResolveSceneInputPipeline(bool velocity, bool bothFace) const;
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
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain, VkFormat depthFormat,
			const BuiltInShaderPasses& passes, const EffectPassDefinition& depthPass,
			const EffectPassDefinition& velocityPass);
		// 생성한 pipeline 리소스를 해제한다.
		void Reset();
	};
}
