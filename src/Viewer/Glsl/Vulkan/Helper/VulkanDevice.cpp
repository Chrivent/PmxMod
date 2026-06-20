#include "VulkanDevice.h"

#include <iostream>
#include <set>
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
		uint64_t highestScore = 0;
		for (const auto candidate : devices) {
			if (!IsDeviceSuitable(candidate))
				continue;
			VkPhysicalDeviceProperties properties{};
			vkGetPhysicalDeviceProperties(candidate, &properties);
			const uint64_t score = ScorePhysicalDevice(properties);
			if (info.physicalDevice != VK_NULL_HANDLE && score <= highestScore)
				continue;
			highestScore = score;
			info.physicalDevice = candidate;
			info.properties = properties;
		}
		if (info.physicalDevice == VK_NULL_HANDLE) {
			std::cerr << "Failed to find a suitable Vulkan physical device.\n";
			return false;
		}
		info.queueFamilies = FindQueueFamilies(info.physicalDevice);
		info.msaaSampleCount = ChooseMsaaSampleCount(info.physicalDevice);
		std::cout << "vulkan_gpu=" << info.properties.deviceName << '\n';
		std::cout << "vulkan_gpu_type=" << GetPhysicalDeviceTypeName(info.properties.deviceType) << '\n';
		return true;
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

	uint64_t VulkanDevice::ScorePhysicalDevice(const VkPhysicalDeviceProperties& properties) {
		uint64_t deviceTypeScore = 0;
		switch (properties.deviceType) {
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				deviceTypeScore = 4;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				deviceTypeScore = 3;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				deviceTypeScore = 2;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				deviceTypeScore = 1;
				break;
			default:
				break;
		}
		return deviceTypeScore * 1'000'000 + properties.limits.maxImageDimension2D;
	}

	const char* VulkanDevice::GetPhysicalDeviceTypeName(const VkPhysicalDeviceType type) {
		switch (type) {
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				return "discrete";
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				return "integrated";
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				return "virtual";
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				return "cpu";
			default:
				return "other";
		}
	}

	VulkanQueueFamilyIndices VulkanDevice::FindQueueFamilies(const VkPhysicalDevice candidate) const {
		VulkanQueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(candidate, &queueFamilyCount, families.data());
		for (uint32_t i = 0; i < queueFamilyCount; i++) {
			VkBool32 presentSupport = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, info.surface, &presentSupport);
			const bool supportsGraphics = (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
			if (supportsGraphics && presentSupport) {
				indices.graphicsFamily = i;
				indices.presentFamily = i;
				indices.hasGraphicsFamily = true;
				indices.hasPresentFamily = true;
				break;
			}
			if (supportsGraphics && !indices.hasGraphicsFamily) {
				indices.graphicsFamily = i;
				indices.hasGraphicsFamily = true;
			}
			if (presentSupport && !indices.hasPresentFamily) {
				indices.presentFamily = i;
				indices.hasPresentFamily = true;
			}
			if (indices.IsComplete())
				break;
		}
		return indices;
	}

	VkSampleCountFlagBits VulkanDevice::ChooseMsaaSampleCount(const VkPhysicalDevice candidate) {
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(candidate, &properties);
		const VkSampleCountFlags supportedSamples =
			properties.limits.framebufferColorSampleCounts &
			properties.limits.framebufferDepthSampleCounts;
		if ((supportedSamples & VK_SAMPLE_COUNT_4_BIT) != 0)
			return VK_SAMPLE_COUNT_4_BIT;
		if ((supportedSamples & VK_SAMPLE_COUNT_2_BIT) != 0)
			return VK_SAMPLE_COUNT_2_BIT;
		return VK_SAMPLE_COUNT_1_BIT;
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
		info.properties = {};
		info.graphicsQueue = VK_NULL_HANDLE;
		info.presentQueue = VK_NULL_HANDLE;
		info.queueFamilies = {};
	}
}
