#pragma once

#include "Viewer/Instance.h"
#include "Viewer/ViewerGeometry.h"
#include "Viewer/Vulkan/VulkanDynamicBufferRing.h"
#include "Viewer/Vulkan/Helper/VulkanBuffer.h"
#include "Viewer/Vulkan/Helper/VulkanDescriptorSet.h"

#include <vector>

namespace Chrivent {
	class VulkanViewer;
	class VulkanDevice;
	struct VulkanMaterial;
	class VulkanPipeline;
	struct VulkanTexture;

	class VulkanInstance : public Instance {
		// 모델 geometry 데이터를 Vulkan vertex/index buffer로 업로드한다.
		bool CreateGeometryBuffers(const VulkanDevice& device);
		// 패스별 uniform buffer ring을 material 개수에 맞춰 생성한다.
		bool SetupConstantRings(const VulkanDevice& device);
		// 모델 material 정보를 Vulkan material 캐시와 descriptor 준비 데이터로 변환한다.
		void LoadMaterials(const VulkanTexture& dummyTexture);
		// 패스별 descriptor set을 생성한다.
		bool CreateDescriptorSets(const VulkanDevice& device, const VulkanPipeline& pipeline);

	public:
		static constexpr size_t kBufferedFrames = 2;
		VulkanViewer* viewer = nullptr;
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

		VulkanInstance();
		~VulkanInstance() override = default;

		// Vulkan 모델 리소스를 해제한다.
		void Clear() override;
		// 모델 데이터를 Vulkan 리소스로 업로드한다.
		bool Setup(Viewer& baseViewer) override;
		// 모델의 갱신된 버텍스 데이터를 Vulkan 리소스에 반영한다.
		void Upload() const override;
	};
}
