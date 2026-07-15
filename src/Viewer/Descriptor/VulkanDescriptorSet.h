#pragma once

#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Pipeline/VulkanPipeline.h"

#include <vector>

namespace Chrivent {
	struct VulkanModelMaterial;
	enum class VulkanPassType {
		Model,
		Edge,
		GroundShadow
	};

	// 모델 패스의 Vulkan uniform 및 텍스처 descriptor set을 관리한다.
	class VulkanDescriptorSet {
		VkDescriptorSet vertexDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> pixelDescriptorSets;
		std::vector<VkDescriptorSet> textureDescriptorSets;
		VkDevice device = VK_NULL_HANDLE;
		VulkanPassType passType = VulkanPassType::Model;

		// uniform buffer와 texture sampler용 descriptor pool을 생성한다.
		bool CreateDescriptorPool(size_t materialCount);
		// pipeline layout에 맞춰 vertex/pixel/texture descriptor set을 할당한다.
		bool AllocateDescriptorSets(const VulkanPipeline& sourcePipeline, size_t materialCount);
		// vertex uniform buffer 정보를 descriptor set에 기록한다.
		void UpdateVertexDescriptorSet(const VulkanBuffer& vertexConstantBuffer, VkDeviceSize vertexConstantRange) const;
		// 재질별 pixel uniform buffer 정보를 descriptor set에 기록한다.
		void UpdatePixelDescriptorSets(const VulkanBuffer& pixelConstantBuffer, VkDeviceSize pixelConstantRange, std::vector<VulkanModelMaterial>& materials) const;
		// 재질별 텍스처 정보를 descriptor set에 기록한다.
		void UpdateTextureDescriptorSets(std::vector<VulkanModelMaterial>& materials) const;

	public:
		VulkanDescriptorSet() = default;
		~VulkanDescriptorSet();

		VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
		VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

		const VkDescriptorSet& GetVertexDescriptorSet() const { return vertexDescriptorSet; }

		// 모델 uniform buffer를 참조하는 descriptor set을 생성하고 갱신한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanPipeline& sourcePipeline,
			const VulkanBuffer& vertexConstantBuffer, VkDeviceSize vertexConstantRange,
			const VulkanBuffer& pixelConstantBuffer, VkDeviceSize pixelConstantRange,
			std::vector<VulkanModelMaterial>& materials, VulkanPassType sourcePassType);
		// descriptor pool과 set 핸들을 해제한다.
		void Reset();
	};
}
