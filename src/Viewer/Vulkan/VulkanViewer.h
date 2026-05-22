#pragma once

#include "../Viewer.h"
#include "Helper/VulkanCommandContext.h"
#include "Helper/VulkanDevice.h"
#include "Helper/VulkanFrameBuffer.h"
#include "Helper/VulkanRenderPass.h"
#include "Helper/VulkanSwapChain.h"
#include "Helper/VulkanSyncObject.h"
#include "VulkanTextureCache.h"

namespace Chrivent {
	struct VulkanMaterial : ViewerMaterial {
		VulkanTexture texture{};
		VulkanTexture sphereTexture{};
		VulkanTexture toonTexture{};

		explicit VulkanMaterial(const Material& sourceMat) : ViewerMaterial(sourceMat) {}
	};
	
	struct VulkanViewerInfo : ViewerInfo {
	};

	class VulkanViewer : public Viewer {
		VulkanDevice device;
		VulkanSwapChain swapChain;
		VulkanRenderPass renderPass;
		VulkanFrameBuffer frameBuffer;
		VulkanCommandContext commandContext;
		VulkanSyncObject syncObject;
		VulkanTextureCache textureCache;

	public:
		VulkanViewer();
		~VulkanViewer() override = default;

		VulkanViewerInfo& GetVulkanInfo() { return static_cast<VulkanViewerInfo&>(GetInfo()); }
		const VulkanViewerInfo& GetVulkanInfo() const { return static_cast<const VulkanViewerInfo&>(GetInfo()); }

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
		VulkanTexture LoadTexture(const std::filesystem::path& texturePath);
	};
}
