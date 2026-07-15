#pragma once

#include "Viewer/Viewer/Viewer.h"
#include "Viewer/Buffer/VulkanDynamicBufferRing.h"
#include "Viewer/Command/VulkanCommandContext.h"
#include "Viewer/Descriptor/VulkanDescriptorSet.h"
#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/RenderTarget/VulkanMsaaColorBuffer.h"
#include "Viewer/RenderTarget/VulkanMsaaDepthBuffer.h"
#include "Viewer/Pipeline/VulkanPipeline.h"
#include "Viewer/SwapChain/VulkanSwapChain.h"
#include "Viewer/Synchronization/VulkanSyncObject.h"
#include "Viewer/PostProcess/VulkanPostProcess.h"
#include "Viewer/Texture/VulkanTextureCache.h"

#include <memory>

namespace Chrivent {
	// 공통 PMX 재질에 Vulkan texture와 descriptor set을 결합한다.
	struct VulkanMaterial : ViewerMaterial {
		VulkanTexture texture{};
		VulkanTexture sphereTexture{};
		VulkanTexture toonTexture{};
		bool textureEnabled = false;
		bool sphereTextureEnabled = false;
		bool toonTextureEnabled = false;
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet edgePixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet groundShadowPixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;

		explicit VulkanMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};
	
	// 중복 Vulkan pipeline 및 descriptor 바인딩을 생략하기 위한 현재 상태를 기록한다.
	struct VulkanBindStateCache {
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkDescriptorSet vertexDescriptorSet = VK_NULL_HANDLE;
		uint32_t vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		uint32_t pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
		VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;
	};

	// 공통 Viewer 계약을 Vulkan command buffer와 스왑체인 흐름으로 구현한다.
	class VulkanViewer : public Viewer {
		std::unique_ptr<VulkanDevice> device;
		VulkanSwapChain swapChain;
		VulkanMsaaColorBuffer msaaColorBuffer;
		VulkanMsaaDepthBuffer msaaDepthBuffer;
		std::unique_ptr<VulkanPipeline> pipeline;
		VulkanCommandContext commandContext;
		std::unique_ptr<VulkanSyncObject> syncObject;
		std::unique_ptr<VulkanTexture> dummyTexture;
		uint32_t currentImageIndex = 0;
		bool frameReady = false;
		bool postProcessSceneInputPassReady = false;
		VulkanBindStateCache bindStateCache;

		VulkanPostProcess postProcess;
		VulkanTextureCache textureCache;

		// swapchain 크기와 포맷에 의존하는 렌더링 리소스를 생성한다.
		bool CreateSwapChainResources();
		// swapchain 재생성 전에 의존 리소스를 역순으로 해제한다.
		void ResetSwapChainResources();

	protected:
		PostProcess& ResolvePostProcess() override { return postProcess; }
		const PostProcess& ResolvePostProcess() const override { return postProcess; }
		
		// 체크된 후처리 효과들을 검증한 뒤 현재 Vulkan 실행 체인과 교체한다.
		bool LoadPostProcessEffectsCore(const std::vector<const EffectDefinition*>& effects) override;
		// 초기 상태의 Vulkan 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstanceCore() override;

	public:
		VulkanViewer();
		~VulkanViewer() override = default;

		const VulkanDevice* GetDevice() const { return device.get(); }
		const VulkanPipeline* GetPipeline() const { return pipeline.get(); }
		const VulkanTexture* GetDummyTexture() const { return dummyTexture.get(); }
		bool ResolveFrameIndex(size_t& result) const {
			if (!syncObject)
				return false;
			result = syncObject->currentFrame;
			return true;
		}
		
		// 현재 프레임 command buffer에 모델 draw indexed 명령을 기록한다.
		void DrawIndexed(const VulkanBuffer& vertexBuffer, const VulkanBuffer& indexBuffer, VkIndexType indexType, size_t firstIndex, size_t indexCount);
		// 현재 프레임 command buffer에 재질 방향성에 맞는 모델 pipeline을 바인딩한다.
		void BindModelPipeline(bool bothFace);
		// 현재 프레임 command buffer에 재질 방향성에 맞는 depth-only pipeline을 바인딩한다.
		void BindDepthOnlyPipeline(bool bothFace);
		// 현재 프레임 command buffer에 재질 방향성에 맞는 장면 속도 pipeline을 바인딩한다.
		void BindSceneVelocityPipeline(bool bothFace);
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
		void ConfigureWindowHints() override;
		// Vulkan 렌더러 리소스를 초기화한다.
		bool Setup() override;
		// 창 크기에 맞춰 Vulkan 스왑체인과 렌더 타깃을 재생성한다.
		bool Resize() override;
		// Vulkan 프레임 렌더링을 시작한다.
		FrameBeginResult BeginFrame() override;
		// Vulkan 프레임을 제출하고 화면에 표시한다.
		FrameEndResult EndFrame() override;
		// Vulkan 후처리 장면 depth와 velocity 입력 패스를 시작한다.
		PostProcessSceneInputBeginResult BeginPostProcessSceneInputPass() override;
		// Vulkan 후처리 장면 입력 패스를 종료한다.
		bool EndPostProcessSceneInputPass() override;
		// Vulkan device에 제출된 작업이 끝날 때까지 기다린다.
		bool WaitIdle() override;
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		VulkanTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);
	};
}
