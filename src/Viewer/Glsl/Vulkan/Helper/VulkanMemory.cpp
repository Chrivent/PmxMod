#include "VulkanMemory.h"

namespace Chrivent {
    bool VulkanMemory::FindMemoryType(const VulkanDevice& deviceInfo, const uint32_t typeFilter,
        const VkMemoryPropertyFlags properties, uint32_t& memoryType) {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(deviceInfo.physicalDevice, &memoryProperties);
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
