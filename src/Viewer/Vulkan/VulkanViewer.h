#pragma once

#include "Viewer/Viewer.h"
#include "Viewer/Shader/ShaderPackage.h"
#include "Viewer/Vulkan/VulkanDynamicBufferRing.h"
#include "Viewer/Vulkan/Helper/VulkanCommandContext.h"
#include "Viewer/Vulkan/Helper/VulkanDescriptorSet.h"
#include "Viewer/Vulkan/Helper/VulkanDevice.h"
#include "Viewer/Vulkan/Helper/VulkanMsaaColorBuffer.h"
#include "Viewer/Vulkan/Helper/VulkanMsaaDepthBuffer.h"
#include "Viewer/Vulkan/Helper/VulkanPipeline.h"
#include "Viewer/Vulkan/Helper/VulkanPostProcess.h"
#include "Viewer/Vulkan/Helper/VulkanSwapChain.h"
#include "Viewer/Vulkan/Helper/VulkanSyncObject.h"
#include "Viewer/Vulkan/VulkanTextureCache.h"

#include <memory>
#include <vector>

namespace Chrivent {
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
	
	struct VulkanBindStateCache {
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkDescriptorSet vertexDescriptorSet = VK_NULL_HANDLE;
		uint32_t vertexDynamicOffset = std::numeric_limits<uint32_t>::max();
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		uint32_t pixelDynamicOffset = std::numeric_limits<uint32_t>::max();
		VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;
	};

	class VulkanViewer : public Viewer {
	public:
		std::shared_ptr<VulkanDevice> device;
		VulkanSwapChain swapChain;
		VulkanMsaaColorBuffer msaaColorBuffer;
		VulkanMsaaDepthBuffer msaaDepthBuffer;
		VulkanPostProcess postProcess;
		std::shared_ptr<VulkanPipeline> pipeline;
		VulkanCommandContext commandContext;
		std::shared_ptr<VulkanSyncObject> syncObject;
		VulkanTextureCache textureCache;
		std::shared_ptr<VulkanTexture> dummyTexture;
		uint32_t currentImageIndex = 0;
		bool frameReady = false;
		VulkanBindStateCache bindStateCache;
		std::vector<EffectDefinition> postProcessEffects;

	private:
		// swapchain 크기와 포맷에 의존하는 렌더링 리소스를 생성한다.
		bool CreateSwapChainResources();
		// swapchain 재생성 전에 의존 리소스를 역순으로 해제한다.
		void ResetSwapChainResources();

	public:
		VulkanViewer();
		~VulkanViewer() override = default;
		
		// 현재 프레임 command buffer에 모델 draw indexed 명령을 기록한다.
		void DrawIndexed(const VulkanBuffer& vertexBuffer, const VulkanBuffer& indexBuffer, VkIndexType indexType, size_t firstIndex, size_t indexCount);
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
		// Vulkan device에 제출된 작업이 끝날 때까지 기다린다.
		void WaitIdle() override;
		// 선택한 포스트 프로세스 효과에 맞춰 Vulkan 스왑체인 의존 리소스를 다시 구성한다.
		bool LoadPostProcessEffect(const EffectDefinition& effect) override;
		// 체크된 포스트 프로세스 효과들에 맞춰 Vulkan 스왑체인 의존 리소스를 다시 구성한다.
		bool LoadPostProcessEffects(const std::vector<const EffectDefinition*>& effects) override;
		// Vulkan 후처리 스왑체인 의존 리소스를 해제하고 기본 렌더 경로로 되돌린다.
		void ClearPostProcessEffect() override;
		// Vulkan 모델 인스턴스를 생성한다.
		std::unique_ptr<Instance> CreateInstance() const override;
		// 텍스처를 캐시에서 찾거나 파일에서 로드해 Vulkan 텍스처로 반환한다.
		VulkanTexture LoadTexture(const std::filesystem::path& texturePath, bool clamp = false);
	};
}
