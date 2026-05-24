#pragma once

#include "VulkanBuffer.h"
#include "VulkanPipeline.h"

namespace Chrivent {
	struct VulkanMaterial;

	struct VulkanDescriptorSetInfo {
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		std::array<VkDescriptorSet, 2> descriptorSets{};
		std::vector<VkDescriptorSet> textureDescriptorSets;
	};

	class VulkanDescriptorSet {
		VulkanDescriptorSetInfo info;
		VkDevice device = VK_NULL_HANDLE;

		// uniform buffer와 texture sampler용 descriptor pool을 생성한다.
		bool CreateDescriptorPool(size_t materialCount);
		// pipeline layout에 맞춰 vertex/pixel/texture descriptor set을 할당한다.
		bool AllocateDescriptorSets(const VulkanPipelineInfo& pipelineInfo, size_t materialCount);
		// uniform buffer 정보를 descriptor set에 기록한다.
		void UpdateDescriptorSets(const VulkanBufferInfo& vertexConstantBuffer, const VulkanBufferInfo& pixelConstantBuffer) const;
		// 재질별 텍스처 정보를 descriptor set에 기록한다.
		void UpdateTextureDescriptorSets(std::vector<VulkanMaterial>& materials) const;

	public:
		VulkanDescriptorSet() = default;
		~VulkanDescriptorSet();

		VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
		VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
		VulkanDescriptorSet(VulkanDescriptorSet&&) = delete;
		VulkanDescriptorSet& operator=(VulkanDescriptorSet&&) = delete;

		VulkanDescriptorSetInfo& GetInfo() { return info; }
		const VulkanDescriptorSetInfo& GetInfo() const { return info; }

		// 모델 uniform buffer를 참조하는 descriptor set을 생성하고 갱신한다.
		bool Initialize(const VulkanDeviceInfo& deviceInfo,
			const VulkanPipelineInfo& pipelineInfo,
			const VulkanBufferInfo& vertexConstantBuffer,
			const VulkanBufferInfo& pixelConstantBuffer,
			std::vector<VulkanMaterial>& materials);
		// descriptor pool과 set 핸들을 해제한다.
		void Destroy();
	};
}
