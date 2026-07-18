#pragma once

#include "Viewer/Error/GraphicsError.h"
#include "Viewer/Shader/SpirvBindingLayout.h"

#include <memory>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace Chrivent {
	class VulkanBuffer;

	// Vulkan 후처리의 descriptor layout, pool, set과 공통 sampler를 소유한다.
	class VulkanPostProcessDescriptors {
		VkDevice device = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayouts[SpirvBindingLayout::setCount]{};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> frameDataDescriptorSets;
		std::vector<VkDescriptorSet> parameterDataDescriptorSets;
		std::vector<VkDescriptorSet> textureDescriptorSets;
		std::vector<VkImageView> textureImageViewCache;
		size_t imageCount = 0;
		size_t passCount = 0;

		// 프레임, 파라미터와 텍스처 descriptor set layout을 생성한다.
		GraphicsResult<void> CreateLayouts();
		// 후처리 공통 sampler를 생성한다.
		GraphicsResult<void> CreateSampler();
		// 필요한 descriptor pool과 set을 생성한다.
		GraphicsResult<void> CreateDescriptorSets();
		// 프레임 및 파라미터 uniform buffer를 descriptor set에 연결한다.
		GraphicsResult<void> BindBuffers(std::span<const std::unique_ptr<VulkanBuffer>> frameDataBuffers,
			std::span<const std::unique_ptr<VulkanBuffer>> parameterDataBuffers,
			VkDeviceSize frameDataSize, VkDeviceSize parameterDataSize,
			VkDeviceSize parameterDataStride) const;

	public:
		VulkanPostProcessDescriptors() = default;
		~VulkanPostProcessDescriptors();

		VulkanPostProcessDescriptors(const VulkanPostProcessDescriptors&) = delete;
		VulkanPostProcessDescriptors& operator=(const VulkanPostProcessDescriptors&) = delete;

		VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }
		VkDescriptorSet GetFrameDataDescriptorSet(uint32_t imageIndex) const;
		VkDescriptorSet GetParameterDataDescriptorSet(uint32_t imageIndex, size_t passIndex) const;
		VkDescriptorSet GetTextureDescriptorSet(uint32_t imageIndex, size_t passIndex) const;

		// 현재 이미지와 패스 수에 맞는 descriptor 리소스인지 확인한다.
		bool IsCompatible(size_t sourceImageCount, size_t sourcePassCount) const;
		// 버퍼와 패스 수에 맞는 Vulkan 후처리 descriptor 리소스를 생성한다.
		GraphicsResult<void> Initialize(VkDevice sourceDevice, size_t sourceImageCount, size_t sourcePassCount,
			std::span<const std::unique_ptr<VulkanBuffer>> frameDataBuffers,
			std::span<const std::unique_ptr<VulkanBuffer>> parameterDataBuffers,
			VkDeviceSize frameDataSize, VkDeviceSize parameterDataSize,
			VkDeviceSize parameterDataStride);
		// 현재 이미지와 패스의 texture 및 sampler descriptor를 갱신한다.
		bool UpdateTextures(uint32_t imageIndex, size_t passIndex,
			std::span<const VkImageView> imageViews);
		// 생성한 descriptor 리소스를 해제한다.
		void Reset();
		// 검증된 다른 descriptor 소유자와 리소스를 교환한다.
		void Swap(VulkanPostProcessDescriptors& other) noexcept;
	};
}
