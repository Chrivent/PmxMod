#pragma once

#include "Viewer/Buffer/VulkanBuffer.h"
#include "Viewer/Buffer/VulkanDynamicBufferRing.h"
#include "Viewer/Descriptor/VulkanDescriptorSet.h"
#include "Viewer/Synchronization/FrameBuffering.h"
#include "Viewer/Texture/VulkanTextureCache.h"

#include <vector>

namespace Chrivent {
	struct Material;

	// 공통 PMX 재질에 Vulkan 텍스처와 descriptor set을 결합한다.
	struct VulkanModelMaterial {
		const Material& material;
		VulkanTexture texture{};
		VulkanTexture sphereTexture{};
		VulkanTexture toonTexture{};
		bool textureEnabled = false;
		bool sphereTextureEnabled = false;
		bool toonTextureEnabled = false;
		VkDescriptorSet pixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet edgePixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet groundShadowPixelDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;

		explicit VulkanModelMaterial(const Material& sourceMaterial) : material(sourceMaterial) {}
	};

	// Vulkan이 한 모델을 그릴 때 사용하는 GPU 리소스를 보관한다.
	struct VulkanModelResources {
		std::vector<VulkanModelMaterial> materials;
		VulkanBuffer vertexBuffers[FrameBuffering::vulkanFramesInFlight];
		VulkanBuffer indexBuffer;
		size_t uniformBufferOffsetAlignment = 1;
		VulkanDynamicBufferRing modelVertexConstantsRing;
		VulkanDynamicBufferRing edgeVertexConstantsRing;
		VulkanDynamicBufferRing groundShadowVertexConstantsRing;
		VulkanDynamicBufferRing modelPixelConstantsRing;
		VulkanDynamicBufferRing edgePixelConstantsRing;
		VulkanDynamicBufferRing groundShadowPixelConstantsRing;
		VulkanDescriptorSet modelDescriptorSet;
		VulkanDescriptorSet edgeDescriptorSet;
		VulkanDescriptorSet groundShadowDescriptorSet;
		VkIndexType indexType = VK_INDEX_TYPE_UINT16;
	};
}
