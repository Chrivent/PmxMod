#pragma once

#include "VulkanBuffer.h"
#include "VulkanPipeline.h"

namespace Chrivent {
	struct VulkanDescriptorSetInfo {
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		std::array<VkDescriptorSet, 2> descriptorSets{};
	};

	class VulkanDescriptorSet {
		VulkanDescriptorSetInfo info;
		VkDevice device = VK_NULL_HANDLE;

		// uniform buffer용 descriptor pool을 생성한다.
		bool CreateDescriptorPool();
		// pipeline layout에 맞춰 vertex/pixel descriptor set을 할당한다.
		bool AllocateDescriptorSets(const VulkanPipelineInfo& pipelineInfo);
		// uniform buffer 정보를 descriptor set에 기록한다.
		void UpdateDescriptorSets(const VulkanBufferInfo& vertexConstantBuffer, const VulkanBufferInfo& pixelConstantBuffer) const;

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
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanPipelineInfo& pipelineInfo, const VulkanBufferInfo& vertexConstantBuffer, const VulkanBufferInfo& pixelConstantBuffer);
		// descriptor pool과 set 핸들을 해제한다.
		void Destroy();
	};
}
