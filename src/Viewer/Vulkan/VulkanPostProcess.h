#pragma once

#include "Viewer/PostProcess.h"
#include "Viewer/Vulkan/Helper/VulkanBuffer.h"
#include "Viewer/Vulkan/Helper/VulkanDevice.h"
#include "Viewer/Vulkan/Helper/VulkanSwapChain.h"

#include <memory>
#include <vector>

namespace Chrivent {
	class VulkanCommandBuffer;
	struct PostProcessFrameData;

	struct VulkanPostProcessResource {
		std::vector<VkImage> images;
		std::vector<VkDeviceMemory> memories;
		std::vector<VkImageView> imageViews;
		std::vector<bool> transientInitialized;
		size_t historyIndex = 0;
		bool historyInitialized = false;
	};

	class VulkanPostProcess : public PostProcess {
		VkDevice device = VK_NULL_HANDLE;
		VkExtent2D targetExtent{};
		VkFormat swapChainFormat = VK_FORMAT_UNDEFINED;
		std::vector<VkImage> sceneImages;
		std::vector<VkDeviceMemory> sceneImageMemories;
		std::vector<VkImageView> sceneImageViews;
		std::vector<VkImage> depthImages;
		std::vector<VkDeviceMemory> depthImageMemories;
		std::vector<VkImageView> depthImageViews;
		std::vector<VkImage> velocityImages;
		std::vector<VkDeviceMemory> velocityImageMemories;
		std::vector<VkImageView> velocityImageViews;
		std::vector<bool> velocityImageInitialized;
		std::vector<VulkanPostProcessResource> resources;
		std::vector<VkDescriptorSet> textureDescriptorSets;
		std::vector<VkDescriptorSet> frameDataDescriptorSets;
		std::vector<std::unique_ptr<VulkanBuffer>> frameDataBuffers;
		VkDescriptorSetLayout descriptorSetLayouts[3]{};
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		std::vector<VkPipeline> pipelines;
		VkSampler sampler = VK_NULL_HANDLE;
		size_t swapChainImageCount = 0;

		// 지정한 색상 texture image, memory와 view를 생성한다.
		bool CreateColorImage(const VulkanDevice& sourceDevice, VkExtent2D extent, VkFormat format,
			VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& memory, VkImageView& imageView) const;
		// 스왑체인 이미지마다 장면 resolve와 sampling에 사용할 이미지를 생성한다.
		bool CreateSceneImages(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 스왑체인 이미지마다 후처리용 단일 샘플 depth 이미지를 생성한다.
		bool CreateDepthImages(const VulkanDevice& sourceDevice,
			const VulkanSwapChain& sourceSwapChain, VkFormat depthFormat);
		// 스왑체인 이미지마다 장면 속도 기록과 sampling에 사용할 RG16F 이미지를 생성한다.
		bool CreateVelocityImages(const VulkanDevice& sourceDevice);
		// 패키지가 선언한 transient/history image를 생성한다.
		bool CreateEffectResources(const VulkanDevice& sourceDevice);
		// 스왑체인 이미지마다 후처리 frame constant buffer를 생성한다.
		bool CreateFrameDataBuffers(const VulkanDevice& sourceDevice);
		// 범용 pass texture/sampler descriptor 리소스를 생성한다.
		bool CreateDescriptors();
		// 현재 pass와 스왑체인 이미지에 대응하는 texture descriptor를 갱신한다.
		void UpdateTextureDescriptorSet(uint32_t imageIndex, size_t passIndex) const;
		// HLSL 후처리 pass 하나를 지정한 출력 크기와 형식의 pipeline으로 만든다.
		bool CreateGraphicsPipeline(const VulkanDevice& sourceDevice, const EffectPassDefinition& pass,
			VkExtent2D extent, VkFormat format, VkPipeline& pipeline) const;
		// 선택된 HLSL 실행 계획으로 fullscreen graphics pipeline들을 생성한다.
		bool CreatePipelines(const VulkanDevice& sourceDevice);
		// 리소스 계획의 Vulkan 색상 형식을 반환한다.
		static VkFormat ResolveResourceFormat(const PostProcessResourcePlan& resource);
		// 리소스 계획의 실제 Vulkan 출력 크기를 반환한다.
		VkExtent2D ResolveResourceExtent(const PostProcessResourcePlan& resource) const;
		// pass 입력 경로에 대응하는 Vulkan image view를 반환한다.
		VkImageView ResolveInputImageView(const PostProcessPassInputRoute& input, uint32_t imageIndex) const;
		// pass 출력 경로에 대응하는 Vulkan image와 view를 반환한다.
		bool ResolveOutputImage(const PostProcessPassRoute& route, uint32_t imageIndex,
			VkImage swapChainImage, VkImageView swapChainImageView, VkImage& image,
			VkImageView& imageView, VkExtent2D& extent, bool& initialized);
		// pass별 descriptor set의 평탄화 인덱스를 반환한다.
		size_t ResolveTextureDescriptorIndex(uint32_t imageIndex, size_t passIndex) const;
		// history 출력 pass가 끝난 뒤 read/write 인덱스를 전환한다.
		void AdvanceHistory(const PostProcessPassRoute& route);
		// Vulkan image 묶음을 안전한 순서로 해제한다.
		void DestroyImages(std::vector<VkImage>& images, std::vector<VkDeviceMemory>& memories,
			std::vector<VkImageView>& imageViews) const;

	public:
		~VulkanPostProcess() override;

		// 현재 스왑체인 이미지에 대응하는 장면 색상 resolve 이미지를 반환한다.
		VkImage ResolveSceneImage(uint32_t imageIndex) const {
			return imageIndex < sceneImages.size() ? sceneImages[imageIndex] : VK_NULL_HANDLE;
		}
		// 현재 스왑체인 이미지에 대응하는 장면 색상 resolve image view를 반환한다.
		VkImageView ResolveSceneImageView(uint32_t imageIndex) const {
			return imageIndex < sceneImageViews.size() ? sceneImageViews[imageIndex] : VK_NULL_HANDLE;
		}
		// 현재 swapchain과 선택된 효과 선언에 맞는 Vulkan 후처리 리소스를 생성한다.
		bool Initialize(const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain,
			VkFormat depthFormat);
		// Vulkan 포스트 프로세스용 단일 샘플 geometry pass를 시작한다.
		bool BeginDepthPass(const VulkanCommandBuffer& commandBuffers, uint32_t imageIndex,
			VkPipeline geometryPipeline, VkExtent2D extent);
		// Vulkan 포스트 프로세스용 단일 샘플 geometry pass를 종료한다.
		bool EndDepthPass(const VulkanCommandBuffer& commandBuffers, uint32_t imageIndex) const;
		// 장면 렌더링을 끝내고 선언된 pass들을 실행해 최종 명령을 기록한다.
		bool EndRecord(const VulkanCommandBuffer& commandBuffers, uint32_t imageIndex, VkImage swapChainImage,
			VkImageView swapChainImageView, VkExtent2D extent, const PostProcessFrameData& frameData,
			bool sceneRenderingEnded = false);
		// 다음 후처리 프레임에서 모든 Vulkan history를 0으로 초기화한다.
		void ResetHistory() override;
		// 생성한 Vulkan 후처리 리소스를 해제한다.
		void Reset() override;
	};
}
