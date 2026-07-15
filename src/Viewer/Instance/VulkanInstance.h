#pragma once

#include "Viewer/Instance/Instance.h"
#include "Viewer/Buffer/VulkanDynamicBufferRing.h"
#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Descriptor/VulkanDescriptorSet.h"
#include "Viewer/Texture/VulkanTextureCache.h"

#include <vector>

namespace Chrivent {
	class VulkanViewer;
	class VulkanDevice;
	class VulkanPipeline;

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

	// 한 모델의 Vulkan 버퍼, 재질과 descriptor 상태를 관리한다.
	class VulkanInstance : public Instance {
		VulkanViewer& viewer;

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
		static constexpr size_t kBufferedFrames = 2;
		std::vector<VulkanMaterial> materials;
		VulkanBuffer vertexBuffers[kBufferedFrames];
		VulkanBuffer indexBuffer;
		size_t uniformBufferOffsetAlignment = 1;
		VulkanDynamicBufferRing modelVertexConstantsRing;
		VulkanDynamicBufferRing edgeVertexConstantsRing;
		VulkanDynamicBufferRing groundShadowVertexConstantsRing;
		VulkanDynamicBufferRing modelPixelConstantsRing;
		VulkanDynamicBufferRing edgePixelConstantsRing;
		VulkanDynamicBufferRing groundShadowPixelConstantsRing;
		VulkanDescriptorSet modelDescriptorSet;
		VulkanDescriptorSet edgeDescriptorSet;
		VulkanDescriptorSet groundShadowDescriptorSet;
		VkIndexType indexType = VK_INDEX_TYPE_UINT16;
		size_t indexCount = 0;

		explicit VulkanInstance(VulkanViewer& sourceViewer);

		// 모델의 갱신된 버텍스 데이터를 Vulkan 리소스에 반영한다.
		bool Upload() const override;
	};
}
