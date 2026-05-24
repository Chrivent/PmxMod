#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"

#include <array>
#include <filesystem>

namespace Chrivent {
	struct VulkanPipelineInfo {
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		std::array<VkDescriptorSetLayout, 3> descriptorSetLayouts{};
	};

	class VulkanPipeline {
		VulkanPipelineInfo info;
		VkDevice device = VK_NULL_HANDLE;

		// 紐⑤뜽 ?곗씠?붿뿉???ъ슜??descriptor set layout?ㅼ쓣 ?앹꽦?쒕떎.
		bool CreateDescriptorSetLayouts();
		// descriptor set layout?ㅼ쓣 臾띠? pipeline layout???앹꽦?쒕떎.
		bool CreatePipelineLayout();
		// 紐⑤뜽 ?뚮뜑留곸슜 graphics pipeline???앹꽦?쒕떎.
		bool CreateGraphicsPipeline(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo, VkRenderPass renderPass, const std::filesystem::path& shaderDir);
		// ?곗씠??stage ?앹꽦 ?뺣낫瑜?留뚮뱺??
		static VkPipelineShaderStageCreateInfo MakeShaderStageInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule);
		// 紐⑤뜽 vertex buffer binding ?뺣낫瑜?留뚮뱺??
		static VkVertexInputBindingDescription MakeVertexBindingDescription();
		// 紐⑤뜽 vertex attribute ?뺣낫瑜?留뚮뱺??
		static std::array<VkVertexInputAttributeDescription, 3> MakeVertexAttributeDescriptions();

	public:
		VulkanPipeline() = default;
		~VulkanPipeline();

		VulkanPipeline(const VulkanPipeline&) = delete;
		VulkanPipeline& operator=(const VulkanPipeline&) = delete;
		VulkanPipeline(VulkanPipeline&&) = delete;
		VulkanPipeline& operator=(VulkanPipeline&&) = delete;

		VulkanPipelineInfo& GetInfo() { return info; }
		const VulkanPipelineInfo& GetInfo() const { return info; }

		// ?뚮뜑 ?⑥뒪? ?ㅼ솑泥댁씤 ?ㅼ젙??留욌뒗 紐⑤뜽 graphics pipeline???앹꽦?쒕떎.
		bool Initialize(const VulkanDeviceInfo& deviceInfo, const VulkanSwapChainInfo& swapChainInfo, VkRenderPass renderPass, const std::filesystem::path& shaderDir);
		// ?앹꽦??pipeline 由ъ냼?ㅻ? ?댁젣?쒕떎.
		void Destroy();
	};
}
