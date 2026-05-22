#include "VulkanDevice.h"

#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

namespace Chrivent {
	bool VulkanDevice::CreateInstance() {
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "PmxMod";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "PmxMod";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		if (!glfwExtensions || glfwExtensionCount == 0) {
			std::cerr << "Failed to get required Vulkan GLFW extensions.\n";
			return false;
		}
		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;
		if (vkCreateInstance(&createInfo, nullptr, &info.vkInstance) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan instance.\n";
			return false;
		}
		return true;
	}

	bool VulkanDevice::CreateSurface(GLFWwindow* window) {
		if (glfwCreateWindowSurface(info.vkInstance, window, nullptr, &info.surface) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan surface.\n";
			return false;
		}
		return true;
	}

	bool VulkanDevice::PickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(info.vkInstance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			std::cerr << "Failed to find a Vulkan physical device.\n";
			return false;
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(info.vkInstance, &deviceCount, devices.data());
		for (const auto candidate : devices) {
			if (!IsDeviceSuitable(candidate))
				continue;
			info.physicalDevice = candidate;
			info.queueFamilies = FindQueueFamilies(candidate);
			return true;
		}
		std::cerr << "Failed to find a suitable Vulkan physical device.\n";
		return false;
	}

	bool VulkanDevice::CreateLogicalDevice() {
		const std::set uniqueQueueFamilies = {
			info.queueFamilies.graphicsFamily,
			info.queueFamilies.presentFamily
		};
		constexpr float queuePriority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		queueCreateInfos.reserve(uniqueQueueFamilies.size());
		for (const uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}
		constexpr VkPhysicalDeviceFeatures deviceFeatures{};
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = queueCreateInfos.size();
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = std::size(kDeviceExtensions);
		createInfo.ppEnabledExtensionNames = kDeviceExtensions;
		if (vkCreateDevice(info.physicalDevice, &createInfo, nullptr, &info.device) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan logical device.\n";
			return false;
		}
		vkGetDeviceQueue(info.device, info.queueFamilies.graphicsFamily, 0, &info.graphicsQueue);
		vkGetDeviceQueue(info.device, info.queueFamilies.presentFamily, 0, &info.presentQueue);
		return true;
	}

	bool VulkanDevice::IsDeviceSuitable(const VkPhysicalDevice candidate) const {
		const VulkanQueueFamilyIndices families = FindQueueFamilies(candidate);
		return families.IsComplete() && CheckDeviceExtensionSupport(candidate);
	}

	VulkanQueueFamilyIndices VulkanDevice::FindQueueFamilies(const VkPhysicalDevice candidate) const {
		VulkanQueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, families.data());
		for (uint32_t i = 0; i < queueFamilyCount; i++) {
			if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
				indices.hasGraphicsFamily = true;
			}
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, info.surface, &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
				indices.hasPresentFamily = true;
			}
			if (indices.IsComplete())
				break;
		}
		return indices;
	}

	bool VulkanDevice::CheckDeviceExtensionSupport(const VkPhysicalDevice candidate) {
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extensionCount, availableExtensions.data());
		std::set<std::string> requiredExtensions(std::begin(kDeviceExtensions), std::end(kDeviceExtensions));
		for (const auto& [extensionName, specVersion] : availableExtensions)
			requiredExtensions.erase(extensionName);
		return requiredExtensions.empty();
	}

	VulkanDevice::~VulkanDevice() {
		Destroy();
	}

	bool VulkanDevice::Initialize(GLFWwindow* window) {
		if (!CreateInstance())
			return false;
		if (!CreateSurface(window))
			return false;
		if (!PickPhysicalDevice())
			return false;
		return CreateLogicalDevice();
	}

	void VulkanDevice::Destroy() {
		if (info.device != VK_NULL_HANDLE) {
			vkDestroyDevice(info.device, nullptr);
			info.device = VK_NULL_HANDLE;
		}
		if (info.surface != VK_NULL_HANDLE && info.vkInstance != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(info.vkInstance, info.surface, nullptr);
			info.surface = VK_NULL_HANDLE;
		}
		if (info.vkInstance != VK_NULL_HANDLE) {
			vkDestroyInstance(info.vkInstance, nullptr);
			info.vkInstance = VK_NULL_HANDLE;
		}
		info.physicalDevice = VK_NULL_HANDLE;
		info.graphicsQueue = VK_NULL_HANDLE;
		info.presentQueue = VK_NULL_HANDLE;
		info.queueFamilies = {};
	}
}
