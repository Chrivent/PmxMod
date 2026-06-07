#pragma once

#include "../Viewer.h"
#include "Assist/VulkanDynamicBufferRing.h"
#include "Helper/VulkanCommandContext.h"
#include "Helper/VulkanDescriptorSet.h"
#include "Helper/VulkanDevice.h"
#include "Helper/VulkanFrameBuffer.h"
#include "Helper/VulkanMsaaColorBuffer.h"
#include "Helper/VulkanMsaaDepthBuffer.h"
#include "Helper/VulkanPipeline.h"
#include "Helper/VulkanRenderPass.h"
#include "Helper/VulkanSwapChain.h"
#include "Helper/VulkanSyncObject.h"
#include "VulkanTextureCache.h"

#include <memory>

namespace Chrivent {
	struct VulkanMaterial : ViewerMaterial {
		VulkanTexture texture{};
		VulkanTexture sphereTexture{};
		VulkanTexture toonTexture{};
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet edgePixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet groundShadowPixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;

		explicit VulkanMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};
	
	struct VulkanViewerInfo : ViewerInfo {
		std::shared_ptr<const VulkanDeviceInfo> deviceInfo;
		std::shared_ptr<const VulkanPipelineInfo> pipelineInfo;
		std::shared_ptr<const VulkanTexture> dummyTexture;
		std::shared_ptr<const VulkanSyncObjectInfo> syncInfo;
	};

	struct VulkanBindStateCache {
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkDescriptorSet vertexDescriptorSet = VK_NULL_HANDLE;
		uint32_t vertexDynamicOffset = (std::numeric_limits<uint32_t>::max)();
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		uint32_t pixelDynamicOffset = (std::numeric_limits<uint32_t>::max)();
		VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;
	};

	class VulkanViewer : public Viewer {
		std::shared_ptr<VulkanDevice> device;
		VulkanSwapChain swapChain;
		VulkanMsaaColorBuffer msaaColorBuffer;
		VulkanMsaaDepthBuffer msaaDepthBuffer;
		VulkanRenderPass renderPass;
		std::shared_ptr<VulkanPipeline> pipeline;
		VulkanFrameBuffer frameBuffer;
		VulkanCommandContext commandContext;
		std::shared_ptr<VulkanSyncObject> syncObject;
		VulkanTextureCache textureCache;
		std::shared_ptr<VulkanTexture> dummyTexture;
		uint32_t currentImageIndex = 0;
		bool frameReady = false;
		VulkanBindStateCache bindStateCache;

	public:
		VulkanViewer();
		~VulkanViewer() override = default;

		VulkanViewerInfo& GetVulkanInfo() { return static_cast<VulkanViewerInfo&>(GetInfo()); }
		const VulkanViewerInfo& GetVulkanInfo() const { return static_cast<const VulkanViewerInfo&>(GetInfo()); }
		
		// 현재 프레임 command buffer에 모델 draw indexed 명령을 기록한다.
		void DrawIndexed(const VulkanBufferInfo& vertexBuffer, const VulkanBufferInfo& indexBuffer, VkIndexType indexType, size_t firstIndex, size_t indexCount);
		// 현재 프레임 command buffer에 재질 방향성에 맞는 모델 pipeline을 바인딩한다.
		void BindModelPipeline(bool bothFace);
		// 현재 프레임 command buffer에 엣지 pipeline을 바인딩한다.
		void BindEdgePipeline();
		// 현재 프레임 command buffer에 지면 그림자 pipeline을 바인딩한다.
		void BindGroundShadowPipeline();
		// 현재 프레임 command buffer에 모델 공통 vertex descriptor set을 바인딩한다.
		void BindModelDescriptorSets(const VulkanDescriptorSet& descriptorSet, uint32_t dynamicOffset);
		// 현재 프레임 command buffer에 재질 pixel descriptor set을 바인딩한다.
		void BindPixelDescriptorSet(VkDescriptorSet descriptorSet, uint32_t dynamicOffset);
		// 현재 프레임 command buffer에 재질 텍스처 descriptor set을 바인딩한다.
		void BindTextureDescriptorSet(VkDescriptorSet descriptorSet);
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
