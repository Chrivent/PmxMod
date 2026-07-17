#pragma once

#include "Viewer/DrawResource/VulkanModelResources.h"
#include "Viewer/Instance/Instance.h"

namespace Chrivent {
	class Viewer;
	class VulkanDrawContext;
	class VulkanDevice;
	class VulkanPipeline;
	class VulkanTextureCache;
	struct VulkanTexture;

	// 한 모델의 Vulkan 버퍼, 재질과 descriptor 상태를 관리한다.
	class VulkanInstance : public Instance {
		const VulkanDevice& device;
		const VulkanPipeline& pipeline;
		VulkanTextureCache& textureCache;
		const VulkanTexture& dummyTexture;
		VulkanDrawContext& drawContext;
		VulkanModelResources modelResources;

		// 모델 geometry 데이터를 Vulkan vertex/index buffer로 업로드한다.
		bool CreateGeometryBuffers(const VulkanDevice& device);
		// 패스별 uniform buffer ring을 material 개수에 맞춰 생성한다.
		bool SetupConstantRings(const VulkanDevice& device);
		// 모델 material 정보를 Vulkan material 캐시와 descriptor 준비 데이터로 변환한다.
		void LoadMaterials(const VulkanTexture& dummyTexture);
		// 패스별 descriptor set을 생성한다.
		bool CreateDescriptorSets(const VulkanDevice& device, const VulkanPipeline& pipeline);
		
	protected:
		// Vulkan 모델 GPU 리소스를 초기 상태로 되돌린다.
		void ResetRendererResources() override;
		// Vulkan 모델 리소스를 생성하고 인스턴스를 초기화한다.
		bool SetupRenderer() override;

	public:
		VulkanInstance(Viewer& sourceViewer, const VulkanDevice& sourceDevice,
			const VulkanPipeline& sourcePipeline, VulkanTextureCache& sourceTextureCache,
			const VulkanTexture& sourceDummyTexture, VulkanDrawContext& sourceDrawContext);

		// 모델의 갱신된 버텍스 데이터를 Vulkan 리소스에 반영한다.
		bool Upload() override;
	};
}
