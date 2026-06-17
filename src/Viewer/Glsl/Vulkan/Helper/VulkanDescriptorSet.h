#pragma once

#include "VulkanBuffer.h"
#include "VulkanPipeline.h"

#include <vector>

namespace Chrivent {
	struct VulkanMaterial;
	enum class VulkanPassType {
		Model,
		Edge,
		GroundShadow
	};

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
		bool AllocateDescriptorSets(const VulkanPipelineInfo& pipelineInfo, size_t materialCount);
		// vertex uniform buffer 정보를 descriptor set에 기록한다.
		void UpdateVertexDescriptorSet(const VulkanBufferInfo& vertexConstantBuffer, VkDeviceSize vertexConstantRange) const;
		// 재질별 pixel uniform buffer 정보를 descriptor set에 기록한다.
		void UpdatePixelDescriptorSets(const VulkanBufferInfo& pixelConstantBuffer, VkDeviceSize pixelConstantRange, std::vector<VulkanMaterial>& materials) const;
		// 재질별 텍스처 정보를 descriptor set에 기록한다.
		void UpdateTextureDescriptorSets(std::vector<VulkanMaterial>& materials) const;

	public:
		VulkanDescriptorSet() = default;
		~VulkanDescriptorSet();

		VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
		VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
		VulkanDescriptorSet(VulkanDescriptorSet&&) = delete;
		VulkanDescriptorSet& operator=(VulkanDescriptorSet&&) = delete;

		const VkDescriptorSet& GetVertexDescriptorSet() const { return vertexDescriptorSet; }

		// 모델 uniform buffer를 참조하는 descriptor set을 생성하고 갱신한다.
		bool Initialize(
			const VulkanDeviceInfo& deviceInfo,
			const VulkanPipelineInfo& pipelineInfo,
			const VulkanBufferInfo& vertexConstantBuffer,
			VkDeviceSize vertexConstantRange,
			const VulkanBufferInfo& pixelConstantBuffer,
			VkDeviceSize pixelConstantRange,
			std::vector<VulkanMaterial>& materials,
			VulkanPassType sourcePassType);
		// descriptor pool과 set 핸들을 해제한다.
		void Destroy();
	};
}
