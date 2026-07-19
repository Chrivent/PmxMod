#include "Viewer/Command/VulkanUploadContext.h"

#include "Viewer/Buffer/VulkanBuffer.h"

#include <limits>

namespace Chrivent {
	void VulkanUploadContext::Reset() {
		retainedBuffers.clear();
		if (device != VK_NULL_HANDLE && fence != VK_NULL_HANDLE)
			vkDestroyFence(device, fence, nullptr);
		if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(device, commandPool, nullptr);
		device = VK_NULL_HANDLE;
		commandPool = VK_NULL_HANDLE;
		commandBuffer = VK_NULL_HANDLE;
		fence = VK_NULL_HANDLE;
		batchRecording = false;
	}

	GraphicsResult<void> VulkanUploadContext::Initialize(const VulkanDevice& sourceDevice) {
		if (device == sourceDevice.GetDevice() && commandPool != VK_NULL_HANDLE
			&& commandBuffer != VK_NULL_HANDLE && fence != VK_NULL_HANDLE)
			return {};
		Reset();
		device = sourceDevice.GetDevice();
		if (device == VK_NULL_HANDLE) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "업로드 context 초기화",
				"Vulkan device를 사용할 수 없습니다"));
		}
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
			| VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = sourceDevice.GetGraphicsQueueFamily();
		VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
		if (result != VK_SUCCESS) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "업로드 command pool 생성",
				"Vulkan 업로드 command pool을 만들지 못했습니다", result, true));
		}
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = commandPool;
		allocateInfo.commandBufferCount = 1;
		result = vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer);
		if (result != VK_SUCCESS) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "업로드 command buffer 할당",
				"Vulkan 업로드 command buffer를 할당하지 못했습니다", result, true));
		}
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		result = vkCreateFence(device, &fenceInfo, nullptr, &fence);
		if (result != VK_SUCCESS) {
			Reset();
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::ResourceCreationFailed, "업로드 fence 생성",
				"Vulkan 업로드 fence를 만들지 못했습니다", result, true));
		}
		return {};
	}

	VulkanUploadContext::~VulkanUploadContext() {
		Reset();
	}

	GraphicsResult<void> VulkanUploadContext::BeginBatch(const VulkanDevice& sourceDevice,
		VkCommandBuffer& targetCommandBuffer) {
		if (batchRecording) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "업로드 batch 시작",
				"Vulkan 업로드 batch가 이미 기록 중입니다"));
		}
		const auto initializeResult = Initialize(sourceDevice);
		if (!initializeResult)
			return std::unexpected(initializeResult.error());
		VkResult result = vkResetCommandBuffer(commandBuffer, 0);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "업로드 command buffer 초기화",
				"Vulkan 업로드 command buffer를 초기화하지 못했습니다", result, true));
		}
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "업로드 command buffer 시작",
				"Vulkan 업로드 command buffer 기록을 시작하지 못했습니다", result, true));
		}
		retainedBuffers.clear();
		batchRecording = true;
		targetCommandBuffer = commandBuffer;
		return {};
	}

	GraphicsResult<void> VulkanUploadContext::SubmitAndWait(const VulkanDevice& sourceDevice) const {
		if (device == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE
			|| fence == VK_NULL_HANDLE || sourceDevice.GetGraphicsQueue() == VK_NULL_HANDLE) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "업로드 command 제출",
				"Vulkan 업로드 context를 사용할 수 없습니다"));
		}
		VkResult result = vkEndCommandBuffer(commandBuffer);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandRecordingFailed, "업로드 command buffer 종료",
				"Vulkan 업로드 command buffer 기록을 끝내지 못했습니다", result, true));
		}
		result = vkResetFences(device, 1, &fence);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::SynchronizationFailed, "업로드 fence 초기화",
				"Vulkan 업로드 fence를 초기화하지 못했습니다", result, true));
		}
		const VkCommandBufferSubmitInfo commandBufferInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = commandBuffer
		};
		const VkSubmitInfo2 submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &commandBufferInfo
		};
		result = vkQueueSubmit2(sourceDevice.GetGraphicsQueue(), 1, &submitInfo, fence);
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::CommandSubmissionFailed, "업로드 command buffer 제출",
				"Vulkan 업로드 command buffer를 제출하지 못했습니다", result, true));
		}
		result = vkWaitForFences(device, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
		if (result != VK_SUCCESS) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::SynchronizationFailed, "업로드 fence 대기",
				"Vulkan 업로드 fence를 기다리지 못했습니다", result, true));
		}
		return {};
	}

	GraphicsResult<void> VulkanUploadContext::SubmitBatch(const VulkanDevice& sourceDevice) {
		if (!batchRecording) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "업로드 batch 제출",
				"제출할 Vulkan 업로드 batch가 없습니다"));
		}
		batchRecording = false;
		const auto submitResult = SubmitAndWait(sourceDevice);
		if (!submitResult)
			return std::unexpected(submitResult.error());
		retainedBuffers.clear();
		return {};
	}

	GraphicsResult<void> VulkanUploadContext::RetainStagingBuffer(
		std::unique_ptr<VulkanBuffer> stagingBuffer) {
		if (!batchRecording || !stagingBuffer) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidState, "업로드 staging buffer 보관",
				"Vulkan 업로드 batch 또는 staging buffer가 올바르지 않습니다"));
		}
		retainedBuffers.emplace_back(std::move(stagingBuffer));
		return {};
	}

	void VulkanUploadContext::CancelBatch() {
		if (!batchRecording)
			return;
		if (commandBuffer != VK_NULL_HANDLE)
			vkEndCommandBuffer(commandBuffer);
		batchRecording = false;
		retainedBuffers.clear();
	}

	GraphicsResult<void> VulkanUploadContext::UploadIndexBuffer(const VulkanDevice& sourceDevice,
		const VkBuffer destination, const VkBuffer source, const VkDeviceSize size) {
		if (destination == VK_NULL_HANDLE || source == VK_NULL_HANDLE || size == 0) {
			return std::unexpected(MakeGraphicsError(GraphicsApi::Vulkan,
				GraphicsErrorCode::InvalidArgument, "index buffer 업로드",
				"복사할 Vulkan index buffer 또는 크기가 올바르지 않습니다"));
		}
		VkCommandBuffer targetCommandBuffer = VK_NULL_HANDLE;
		const auto beginResult = BeginBatch(sourceDevice, targetCommandBuffer);
		if (!beginResult)
			return std::unexpected(beginResult.error());
		const VkBufferCopy copyRegion{ .size = size };
		vkCmdCopyBuffer(targetCommandBuffer, source, destination, 1, &copyRegion);
		const VkBufferMemoryBarrier2 barrier{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
			.dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = destination,
			.offset = 0,
			.size = size
		};
		const VkDependencyInfo dependencyInfo{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &barrier
		};
		vkCmdPipelineBarrier2(targetCommandBuffer, &dependencyInfo);
		return SubmitBatch(sourceDevice);
	}
}
