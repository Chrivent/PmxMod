#include "Viewer/Device/VulkanDevice.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <vector>

namespace Chrivent {
	bool VulkanDevice::CreateInstance() {
		uint32_t loaderVersion = VK_API_VERSION_1_0;
		const auto enumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
			vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
		if (enumerateInstanceVersion && enumerateInstanceVersion(&loaderVersion) != VK_SUCCESS)
			return false;
		if (loaderVersion < VK_API_VERSION_1_3) {
			std::cerr << "Vulkan 1.3 or newer is required.\n";
			return false;
		}
		instanceApiVersion = VK_API_VERSION_1_3;
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "PmxMod";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "PmxMod";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = instanceApiVersion;
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
		if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan instance.\n";
			return false;
		}
		return true;
	}

	bool VulkanDevice::CreateSurface(GLFWwindow* window) {
		if (glfwCreateWindowSurface(vkInstance, window, nullptr, &surface) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan surface.\n";
			return false;
		}
		return true;
	}

	bool VulkanDevice::PickPhysicalDevice() {
		uint32_t deviceCount = 0;
		if (vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr) != VK_SUCCESS)
			return false;
		if (deviceCount == 0) {
			std::cerr << "Failed to find a Vulkan physical device.\n";
			return false;
		}
		std::vector<VkPhysicalDevice> devices(deviceCount);
		if (vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data()) != VK_SUCCESS)
			return false;
		devices.resize(deviceCount);
		uint64_t highestScore = 0;
		for (const auto candidate : devices) {
			if (!IsDeviceSuitable(candidate))
				continue;
			VkPhysicalDeviceProperties newProperties{};
			vkGetPhysicalDeviceProperties(candidate, &newProperties);
			const uint64_t score = ScorePhysicalDevice(newProperties);
			if (physicalDevice != VK_NULL_HANDLE && score <= highestScore)
				continue;
			highestScore = score;
			physicalDevice = candidate;
			properties = newProperties;
		}
		if (physicalDevice == VK_NULL_HANDLE) {
			std::cerr << "Failed to find a suitable Vulkan physical device.\n";
			return false;
		}
		queueFamilies = FindQueueFamilies(physicalDevice);
		msaaSampleCount = ChooseMsaaSampleCount(physicalDevice);
		UpdateCapabilities();
		capabilities.Print();
		return true;
	}

	bool VulkanDevice::CreateLogicalDevice() {
		const std::set uniqueQueueFamilies = {
			queueFamilies.graphicsFamily,
			queueFamilies.presentFamily
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
		VkPhysicalDeviceVulkan13Features vulkan13Features{};
		vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vulkan13Features.dynamicRendering = VK_TRUE;
		vulkan13Features.synchronization2 = VK_TRUE;
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pNext = &vulkan13Features;
		createInfo.queueCreateInfoCount = queueCreateInfos.size();
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.enabledExtensionCount = std::size(kDeviceExtensions);
		createInfo.ppEnabledExtensionNames = kDeviceExtensions;
		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
			std::cerr << "Failed to create Vulkan logical device.\n";
			return false;
		}
		vkGetDeviceQueue(device, queueFamilies.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(device, queueFamilies.presentFamily, 0, &presentQueue);
		return true;
	}

	bool VulkanDevice::IsDeviceSuitable(const VkPhysicalDevice candidate) const {
		const VulkanQueueFamilyIndices families = FindQueueFamilies(candidate);
		VkPhysicalDeviceProperties candidateProperties{};
		vkGetPhysicalDeviceProperties(candidate, &candidateProperties);
		if (!families.IsComplete() || candidateProperties.apiVersion < VK_API_VERSION_1_3 ||
			!CheckDeviceExtensionSupport(candidate))
			return false;
		VkPhysicalDeviceVulkan13Features vulkan13Features{};
		vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		VkPhysicalDeviceFeatures2 features{};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &vulkan13Features;
		vkGetPhysicalDeviceFeatures2(candidate, &features);
		if (vulkan13Features.dynamicRendering != VK_TRUE || vulkan13Features.synchronization2 != VK_TRUE)
			return false;
		uint32_t formatCount = 0;
		uint32_t presentModeCount = 0;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface, &formatCount, nullptr) != VK_SUCCESS
			|| vkGetPhysicalDeviceSurfacePresentModesKHR(
				candidate, surface, &presentModeCount, nullptr) != VK_SUCCESS)
			return false;
		return formatCount > 0 && presentModeCount > 0;
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

	const char* VulkanDevice::ResolvePhysicalDeviceTypeName(const VkPhysicalDeviceType type) {
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
			if (vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &presentSupport) != VK_SUCCESS)
				continue;
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

	uint32_t VulkanDevice::ResolveMaximumMsaaSampleCount(const VkPhysicalDevice candidate) {
		VkPhysicalDeviceProperties candidateProperties{};
		vkGetPhysicalDeviceProperties(candidate, &candidateProperties);
		const VkSampleCountFlags supportedSamples = candidateProperties.limits.framebufferColorSampleCounts
			& candidateProperties.limits.framebufferDepthSampleCounts;
		constexpr VkSampleCountFlagBits sampleCounts[] = {
			VK_SAMPLE_COUNT_64_BIT,
			VK_SAMPLE_COUNT_32_BIT,
			VK_SAMPLE_COUNT_16_BIT,
			VK_SAMPLE_COUNT_8_BIT,
			VK_SAMPLE_COUNT_4_BIT,
			VK_SAMPLE_COUNT_2_BIT
		};
		for (const VkSampleCountFlagBits sampleCount : sampleCounts) {
			if ((supportedSamples & sampleCount) != 0)
				return static_cast<uint32_t>(sampleCount);
		}
		return 1;
	}

	bool VulkanDevice::CheckDeviceExtensionSupport(const VkPhysicalDevice candidate) {
		uint32_t extensionCount = 0;
		if (vkEnumerateDeviceExtensionProperties(
			candidate, nullptr, &extensionCount, nullptr) != VK_SUCCESS)
			return false;
		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		if (vkEnumerateDeviceExtensionProperties(
			candidate, nullptr, &extensionCount, availableExtensions.data()) != VK_SUCCESS)
			return false;
		availableExtensions.resize(extensionCount);
		std::set<std::string> requiredExtensions(std::begin(kDeviceExtensions), std::end(kDeviceExtensions));
		for (const auto& [extensionName, specVersion] : availableExtensions)
			requiredExtensions.erase(extensionName);
		return requiredExtensions.empty();
	}

	std::string VulkanDevice::ResolveVersionName(const uint32_t version) {
		return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
			std::to_string(VK_API_VERSION_MINOR(version)) + "." +
			std::to_string(VK_API_VERSION_PATCH(version));
	}

	void VulkanDevice::UpdateCapabilities() {
		VkPhysicalDeviceVulkan12Features vulkan12Features{};
		vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		VkPhysicalDeviceVulkan13Features vulkan13Features{};
		vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		vulkan12Features.pNext = properties.apiVersion >= VK_API_VERSION_1_3 ? &vulkan13Features : nullptr;
		VkPhysicalDeviceFeatures2 features{};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &vulkan12Features;
		vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
		capabilities.apiName = "Vulkan";
		capabilities.apiVersion = ResolveVersionName(properties.apiVersion);
		capabilities.shaderVersion = "HLSL 6 / SPIR-V 1.6";
		capabilities.gpuName = properties.deviceName;
		capabilities.gpuType = ResolvePhysicalDeviceTypeName(properties.deviceType);
		capabilities.maxSampleCount = ResolveMaximumMsaaSampleCount(physicalDevice);
		capabilities.activeSampleCount = static_cast<uint32_t>(msaaSampleCount);
		capabilities.uniformBufferAlignment = properties.limits.minUniformBufferOffsetAlignment;
		capabilities.maxTextureBindings = properties.limits.maxPerStageDescriptorSampledImages;
		capabilities.shaderModelMajor = 6;
		capabilities.supportsTimelineSynchronization = vulkan12Features.timelineSemaphore == VK_TRUE;
		capabilities.supportsDynamicRendering = vulkan13Features.dynamicRendering == VK_TRUE;
		capabilities.supportsEnhancedBarriers = vulkan13Features.synchronization2 == VK_TRUE;
	}

	VulkanDevice::~VulkanDevice() {
		Shutdown();
	}

	bool VulkanDevice::Initialize(GLFWwindow* window) {
		Shutdown();
		if (!CreateInstance())
			return false;
		if (!CreateSurface(window))
			return false;
		if (!PickPhysicalDevice())
			return false;
		return CreateLogicalDevice();
	}

	void VulkanDevice::Shutdown() {
		if (device != VK_NULL_HANDLE) {
			vkDestroyDevice(device, nullptr);
			device = VK_NULL_HANDLE;
		}
		if (surface != VK_NULL_HANDLE && vkInstance != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(vkInstance, surface, nullptr);
			surface = VK_NULL_HANDLE;
		}
		if (vkInstance != VK_NULL_HANDLE) {
			vkDestroyInstance(vkInstance, nullptr);
			vkInstance = VK_NULL_HANDLE;
		}
		physicalDevice = VK_NULL_HANDLE;
		properties = {};
		graphicsQueue = VK_NULL_HANDLE;
		presentQueue = VK_NULL_HANDLE;
		queueFamilies = {};
		capabilities = {};
		instanceApiVersion = VK_API_VERSION_1_0;
	}
}
