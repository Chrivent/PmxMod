#include "Viewer/Memory/VulkanMemory.h"

namespace Chrivent {
    bool VulkanMemory::FindMemoryType(const VulkanDevice& sourceDevice, const uint32_t typeFilter,
        const VkMemoryPropertyFlags properties, uint32_t& memoryType) {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(sourceDevice.GetPhysicalDevice(), &memoryProperties);
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
            if ((typeFilter & 1 << i) != 0 &&
                (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                memoryType = i;
                return true;
            }
        }
        return false;
    }
}
