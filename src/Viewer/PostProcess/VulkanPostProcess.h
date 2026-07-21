#pragma once

#include "Viewer/PostProcess/PostProcess.h"
#include "Viewer/PostProcess/VulkanPostProcessPipelines.h"
#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Descriptor/VulkanPostProcessDescriptors.h"
#include "Viewer/Device/VulkanDevice.h"
#include "Viewer/Error/GraphicsError.h"
#include "Viewer/RenderTarget/VulkanPostProcessTarget.h"
#include "Viewer/SwapChain/VulkanSwapChain.h"

#include <memory>
#include <vector>

namespace Chrivent {
	class VulkanCommandContext;
	struct PostProcessFrameData;

	// 공통 실행 계획을 Vulkan image와 그래픽 파이프라인으로 기록한다.
	class VulkanPostProcess : public PostProcess {
		VkDevice device = VK_NULL_HANDLE;
		VkExtent2D targetExtent{};
		VkFormat swapChainFormat = VK_FORMAT_UNDEFINED;
		VulkanPostProcessTarget sceneTarget;
		VulkanPostProcessTarget depthTarget;
		VulkanPostProcessTarget velocityTarget;
		std::vector<VulkanPostProcessTarget> resources;
		std::vector<std::unique_ptr<VulkanBuffer>> frameDataBuffers;
		std::vector<std::unique_ptr<VulkanBuffer>> parameterDataBuffers;
		VulkanPostProcessDescriptors descriptors;
		VulkanPostProcessPipelines pipelines;
		size_t swapChainImageCount = 0;
		VkDeviceSize parameterDataStride = 0;

		// 스왑체인 이미지마다 장면 resolve와 sampling에 사용할 이미지를 생성한다.
		GraphicsError::Result<void> CreateSceneImages(
			const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain);
		// 스왑체인 이미지마다 장면 속도 기록과 sampling에 사용할 RG16F 이미지를 생성한다.
		GraphicsError::Result<void> CreateVelocityImages(const VulkanDevice& sourceDevice);
		// 패키지가 선언한 transient/history image를 생성한다.
		GraphicsError::Result<void> CreateEffectResources(const VulkanDevice& sourceDevice);
		// 스왑체인 이미지마다 후처리 frame constant buffer를 생성한다.
		GraphicsError::Result<void> CreateFrameDataBuffers(const VulkanDevice& sourceDevice);
		// 스왑체인 이미지마다 모든 pass 파라미터를 보관할 b1 buffer를 생성한다.
		GraphicsError::Result<void> CreateParameterDataBuffers(const VulkanDevice& sourceDevice);
		// 현재 pass와 스왑체인 이미지에 대응하는 texture descriptor를 갱신한다.
		bool UpdateTextureDescriptorSet(uint32_t imageIndex, size_t passIndex);
		// 새 프레임의 Vulkan 이미지 상태 변경을 제출 전 임시 상태로 기록한다.
		void BeginImageStateFrame();
		// 선택된 HLSL 실행 계획으로 fullscreen graphics pipeline들을 생성한다.
		GraphicsError::Result<void> CreatePipelines(const VulkanDevice& sourceDevice);
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
		// 검증을 마친 다른 Vulkan 후처리 객체와 GPU 리소스를 교환한다.
		void SwapResources(VulkanPostProcess& other) noexcept;

	public:
		~VulkanPostProcess() override;

		VkImage TryGetSceneImage(const uint32_t imageIndex) const {
			return sceneTarget.TryGetImage(imageIndex);
		}
		VkImageView TryGetSceneImageView(const uint32_t imageIndex) const {
			return sceneTarget.TryGetImageView(imageIndex);
		}
		
		// 현재 swapchain과 선택된 효과 선언에 맞는 Vulkan 후처리 target을 생성한다.
		GraphicsError::Result<void> InitializeTargets(
			const VulkanDevice& sourceDevice, const VulkanSwapChain& sourceSwapChain, VkFormat depthFormat);
		// 효과 선택 변경에 맞춰 Vulkan 후처리 리소스와 pipeline을 원자적으로 교체한다.
		GraphicsError::Result<void> Configure(const VulkanDevice& sourceDevice,
			const VulkanSwapChain& sourceSwapChain,
			VkFormat depthFormat, PreparedEffects preparedEffects);
		// Vulkan 후처리 장면 depth와 velocity 입력 geometry pass를 시작한다.
		GraphicsError::Result<void> BeginSceneInputPass(const VulkanCommandContext& commandContext,
			uint32_t imageIndex, VkExtent2D extent);
		// Vulkan 후처리 장면 입력 geometry pass를 종료한다.
		GraphicsError::Result<void> EndSceneInputPass(
			const VulkanCommandContext& commandContext, uint32_t imageIndex);
		// 선언된 pass들을 실행해 swapchain 출력까지 기록한다.
		GraphicsError::Result<void> Draw(const VulkanCommandContext& commandContext, uint32_t imageIndex,
			VkImage swapChainImage, VkImageView swapChainImageView, const PostProcessFrameData& frameData);
		// 제출된 프레임의 Vulkan 이미지 상태 변경을 확정한다.
		void CommitImageStateFrame();
		// 제출되지 않은 프레임의 Vulkan 이미지 상태 변경을 버린다.
		void DiscardImageStateFrame();
		// 크기에 종속된 Vulkan 후처리 target과 descriptor만 해제한다.
		void ResetTargets();
		// 생성한 Vulkan 후처리 리소스를 해제한다.
		void ResetResources() override;
	};
}
