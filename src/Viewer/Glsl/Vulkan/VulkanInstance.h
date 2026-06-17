#pragma once

#include "../../Instance.h"
#include "../../ViewerGeometry.h"
#include "VulkanDynamicBufferRing.h"
#include "Helper/VulkanBuffer.h"
#include "Helper/VulkanDescriptorSet.h"

#include <array>
#include <vector>

namespace Chrivent {
	class VulkanViewer;
	struct VulkanMaterial;

	using VulkanVertex = ViewerVertex;

	struct VulkanInstanceInfo : InstanceInfo {
		static constexpr size_t kBufferedFrames = 2;
		VulkanViewer* viewer = nullptr;
		std::vector<VulkanMaterial> materials;
		std::array<VulkanBuffer, kBufferedFrames> vertexBuffers;
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
	};

	class VulkanInstance : public Instance {
	public:
		VulkanInstance();
		~VulkanInstance() override = default;

		// Vulkan 모델 리소스를 해제한다.
		void Clear() override;
		// 모델 데이터를 Vulkan 리소스로 업로드한다.
		bool Setup(Viewer& baseViewer) override;
		// 모델의 갱신된 버텍스 데이터를 Vulkan 리소스에 반영한다.
		void Update() const override;
	};
}
