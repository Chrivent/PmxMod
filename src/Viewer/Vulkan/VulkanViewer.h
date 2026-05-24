#pragma once

#include "../Viewer.h"
#include "Helper/VulkanCommandContext.h"
#include "Helper/VulkanColorBuffer.h"
#include "Helper/VulkanDepthBuffer.h"
#include "Helper/VulkanDescriptorSet.h"
#include "Helper/VulkanDevice.h"
#include "Helper/VulkanFrameBuffer.h"
#include "Helper/VulkanPipeline.h"
#include "Helper/VulkanRenderPass.h"
#include "Helper/VulkanSwapChain.h"
#include "Helper/VulkanSyncObject.h"
#include "VulkanTextureCache.h"

namespace Chrivent {
	struct VulkanMaterial : ViewerMaterial {
		VulkanTexture texture{};
		VulkanTexture sphereTexture{};
		VulkanTexture toonTexture{};
		std::unique_ptr<VulkanBuffer> pixelConstantBuffer;
		std::unique_ptr<VulkanBuffer> edgePixelConstantBuffer;
		std::unique_ptr<VulkanBuffer> groundShadowPixelConstantBuffer;
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet edgePixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet groundShadowPixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;

		explicit VulkanMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};
	
	struct VulkanViewerInfo : ViewerInfo {};

	class VulkanViewer : public Viewer {
		VulkanDevice device;
		VulkanSwapChain swapChain;
		VulkanColorBuffer colorBuffer;
		VulkanDepthBuffer depthBuffer;
		VulkanRenderPass renderPass;
		VulkanPipeline pipeline;
		VulkanFrameBuffer frameBuffer;
		VulkanCommandContext commandContext;
		VulkanSyncObject syncObject;
		VulkanTextureCache textureCache;
		VulkanTexture dummyTexture;
		uint32_t currentImageIndex = 0;
		bool frameReady = false;

	public:
		VulkanViewer();
		~VulkanViewer() override = default;

		const VulkanDeviceInfo& GetDeviceInfo() const { return device.GetInfo(); }
		const VulkanPipelineInfo& GetPipelineInfo() const { return pipeline.GetInfo(); }
		const VulkanTexture& GetDummyTexture() const { return dummyTexture; }
		
		// 현재 프레임 command buffer에 모델 draw indexed 명령을 기록한다.
		void DrawIndexed(const VulkanBufferInfo& vertexBuffer, const VulkanBufferInfo& indexBuffer, VkIndexType indexType, size_t firstIndex, size_t indexCount) const;
		// 현재 프레임 command buffer에 재질 방향성에 맞는 모델 pipeline을 바인딩한다.
		void BindModelPipeline(bool bothFace) const;
		// 현재 프레임 command buffer에 엣지 pipeline을 바인딩한다.
		void BindEdgePipeline() const;
		// 현재 프레임 command buffer에 지면 그림자 pipeline을 바인딩한다.
		void BindGroundShadowPipeline() const;
		// 현재 프레임 command buffer에 모델 공통 vertex descriptor set을 바인딩한다.
		void BindModelDescriptorSets(const VulkanDescriptorSetInfo& descriptorSetInfo) const;
		// 현재 프레임 command buffer에 재질 pixel descriptor set을 바인딩한다.
		void BindPixelDescriptorSet(VkDescriptorSet descriptorSet) const;
		// 현재 프레임 command buffer에 재질 텍스처 descriptor set을 바인딩한다.
		void BindTextureDescriptorSet(VkDescriptorSet descriptorSet) const;
		// Vulkan 렌더링에 필요한 GLFW 윈도우 힌트를 설정한다.
		void ConfigureGlfwHints() override;
		// Vulkan 렌더러 리소스를 초기화한다.
		bool Setup() override;
		// 창 크기에 맞춰 Vulkan 스왑체인과 렌더 타깃을 재생성한다.
		bool Resize() override;
		// Vulkan 프레임 렌더링을 시작한다.
		void BeginFrame() override;
		// Vulkan 프레임을 제출하고 화면에 표시한다.
		bool EndFrame() override;
		// Vulkan 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstance() const override;
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		VulkanTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);
	};
}
