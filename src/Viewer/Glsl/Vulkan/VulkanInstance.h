#pragma once

#include "../../Instance.h"
#include "VulkanDynamicBufferRing.h"
#include "Helper/VulkanBuffer.h"
#include "Helper/VulkanDescriptorSet.h"

#include <array>
#include <vector>

namespace Chrivent {
	class VulkanViewer;
	struct ModelGeometryData;
	struct VulkanMaterial;

	struct VulkanVertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;
	};

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
		// 모델의 현재 버텍스 배열을 Vulkan 입력 레이아웃에 맞게 변환한다.
		static void FillVertices(const ModelGeometryData& geometryData, bool useUpdateData, std::vector<VulkanVertex>& vertices);
		// PMX 인덱스 데이터를 Vulkan에서 사용할 수 있는 index buffer 데이터로 변환한다.
		static bool BuildIndexData(const ModelGeometryData& geometryData, std::vector<char>& indices, VkIndexType& indexType);

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
