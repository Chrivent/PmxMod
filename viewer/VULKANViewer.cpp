#include "VULKANViewer.h"

bool FindMemoryTypeIndex(const vk::PhysicalDeviceMemoryProperties& memProps,
        const uint32_t typeBits, const vk::MemoryPropertyFlags reqMask, uint32_t* typeIndex) {
    if (typeIndex == nullptr)
        return false;
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((typeBits >> i & 1) == 1) {
            if ((memProps.memoryTypes[i].propertyFlags & reqMask) == reqMask) {
                *typeIndex = i;
                return true;
            }
        }
    }
    return false;
}

void SetImageLayout(const vk::CommandBuffer cmdBuf, const vk::Image image,
		const vk::ImageLayout oldImageLayout, const vk::ImageLayout newImageLayout,
		const vk::ImageSubresourceRange& subresourceRange) {
	auto imageMemoryBarrier = vk::ImageMemoryBarrier()
			.setOldLayout(oldImageLayout)
			.setNewLayout(newImageLayout)
			.setImage(image)
			.setSubresourceRange(subresourceRange);
	switch (oldImageLayout) {
		case vk::ImageLayout::eUndefined:
			imageMemoryBarrier.setSrcAccessMask(vk::AccessFlags());
			break;
		case vk::ImageLayout::eTransferDstOptimal:
			imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			break;
		case vk::ImageLayout::ePreinitialized:
			imageMemoryBarrier.setSrcAccessMask(vk::AccessFlagBits::eHostWrite);
			break;
		default:
			break;
	}
	switch (newImageLayout) {
		case vk::ImageLayout::eShaderReadOnlyOptimal:
			imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			break;
		case vk::ImageLayout::eTransferSrcOptimal:
			imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferRead);
			break;
		case vk::ImageLayout::eTransferDstOptimal:
			imageMemoryBarrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			break;
		default:
			break;
	}
	vk::PipelineStageFlags srcStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
	vk::PipelineStageFlags destStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
	if (oldImageLayout == vk::ImageLayout::eUndefined &&
	    newImageLayout == vk::ImageLayout::eTransferDstOptimal) {
		srcStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
		destStageFlags = vk::PipelineStageFlagBits::eTransfer;
	} else if (oldImageLayout == vk::ImageLayout::eTransferDstOptimal &&
	           newImageLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		srcStageFlags = vk::PipelineStageFlagBits::eTransfer;
		destStageFlags = vk::PipelineStageFlagBits::eFragmentShader;
	}
	cmdBuf.pipelineBarrier(
		srcStageFlags,
		destStageFlags,
		vk::DependencyFlags(),
		0, nullptr,
		0, nullptr,
		1, &imageMemoryBarrier
	);
}
