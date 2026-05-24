#pragma once

#include "../Instance.h"
#include "Helper/VulkanBuffer.h"

#include <glm/glm.hpp>

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
		VulkanViewer* viewer = nullptr;
		std::vector<VulkanMaterial> materials;
		VulkanBuffer vertexBuffer;
		VulkanBuffer indexBuffer;
		VkIndexType indexType = VK_INDEX_TYPE_UINT16;
		size_t indexCount = 0;
	};

	class VulkanInstance : public Instance {
		// 모델의 현재 버텍스 배열을 Vulkan 입력 레이아웃에 맞게 변환한다.
		static std::vector<VulkanVertex> MakeVertices(const ModelGeometryData& geometryData, bool useUpdateData);
		// PMX 인덱스 바이트 크기를 Vulkan index type으로 변환한다.
		static bool GetIndexType(size_t indexElementSize, VkIndexType& indexType);

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
