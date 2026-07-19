#pragma once

#include "Viewer/DrawResource/VulkanModelResources.h"
#include "Viewer/Instance/Instance.h"

namespace Chrivent {
	class VulkanDrawContext;
	class VulkanDevice;
	class VulkanPipeline;
	class VulkanTextureCache;
	class VulkanUploadContext;
	struct VulkanTexture;

	// 한 모델의 Vulkan 버퍼, 재질과 descriptor 상태를 관리한다.
	class VulkanInstance : public Instance {
		const VulkanDevice& device;
		const VulkanPipeline& pipeline;
		VulkanUploadContext& uploadContext;
		VulkanTextureCache& textureCache;
		const VulkanTexture& dummyTexture;
		VulkanDrawContext& drawContext;
		VulkanModelResources modelResources;

		// 모델 geometry 데이터를 Vulkan vertex/index buffer로 업로드한다.
		GraphicsError::Result<void> CreateGeometryBuffers();
		// vertex/pixel uniform buffer ring을 material 개수에 맞춰 생성한다.
		GraphicsError::Result<void> SetupConstantRings();
		// 모델 material 정보를 Vulkan material 캐시와 descriptor 준비 데이터로 변환한다.
		GraphicsError::Result<void> LoadMaterials();
		// 렌더 패스별 descriptor set을 공용 uniform buffer ring에 연결한다.
		GraphicsError::Result<void> CreateDescriptorSets();
		
	protected:
		// Vulkan 모델 GPU 리소스를 초기 상태로 되돌린다.
		void ResetRendererResources() override;
		// Vulkan 모델 리소스를 생성하고 인스턴스를 초기화한다.
		GraphicsError::Result<void> SetupRenderer() override;
		// 모델의 갱신된 버텍스 데이터를 Vulkan 리소스에 반영한다.
		GraphicsError::Result<void> UploadCore() override;

	public:
		VulkanInstance(const VulkanDevice& sourceDevice,
			const VulkanPipeline& sourcePipeline, VulkanUploadContext& sourceUploadContext,
			VulkanTextureCache& sourceTextureCache, const VulkanTexture& sourceDummyTexture,
			VulkanDrawContext& sourceDrawContext);
	};
}
