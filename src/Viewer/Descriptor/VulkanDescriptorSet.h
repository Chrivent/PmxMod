#pragma once

#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Pipeline/VulkanPipeline.h"

#include <vector>

namespace Chrivent {
	struct VulkanModelMaterial;

	// 렌더 패스의 Vulkan uniform 및 선택적 재질 텍스처 descriptor set을 관리한다.
	class VulkanDescriptorSet {
		VkDescriptorSet vertexDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> textureDescriptorSets;
		VkDevice device = VK_NULL_HANDLE;

		// uniform buffer와 texture sampler용 descriptor pool을 생성한다.
		GraphicsResult<void> CreateDescriptorPool(size_t textureDescriptorCount);
		// pipeline layout에 맞춰 vertex/pixel/texture descriptor set을 할당한다.
		GraphicsResult<void> AllocateDescriptorSets(const VulkanPipeline& sourcePipeline,
			size_t textureDescriptorCount);
		// vertex uniform buffer 정보를 descriptor set에 기록한다.
		void UpdateVertexDescriptorSet(const VulkanBuffer& vertexConstantBuffer, VkDeviceSize vertexConstantRange) const;
		// 패스 공통 pixel uniform buffer 정보를 descriptor set에 기록한다.
		void UpdatePixelDescriptorSet(const VulkanBuffer& pixelConstantBuffer, VkDeviceSize pixelConstantRange) const;
		// 재질별 텍스처 정보를 descriptor set에 기록한다.
		GraphicsResult<void> UpdateTextureDescriptorSets(std::vector<VulkanModelMaterial>& materials) const;

	public:
		VulkanDescriptorSet() = default;
		~VulkanDescriptorSet();

		VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
		VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

		VkDescriptorSet GetVertexDescriptorSet() const { return vertexDescriptorSet; }
		VkDescriptorSet GetPixelDescriptorSet() const { return pixelDescriptorSet; }

		// 모델 uniform buffer를 참조하는 descriptor set을 생성하고 갱신한다.
		GraphicsResult<void> Initialize(const VulkanDevice& sourceDevice,
			const VulkanPipeline& sourcePipeline,
			const VulkanBuffer& vertexConstantBuffer, VkDeviceSize vertexConstantRange,
			const VulkanBuffer& pixelConstantBuffer, VkDeviceSize pixelConstantRange,
			std::vector<VulkanModelMaterial>& materials, bool materialTexturesRequired);
		// descriptor pool과 set 핸들을 해제한다.
		void Reset();
	};
}
