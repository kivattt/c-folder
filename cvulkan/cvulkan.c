#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "cvulkan.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
	VkBuffer buffer;
	VkDeviceMemory memory;
} AllocatedBuffer;

typedef struct {
	VkCommandPool command_pool;
	VkCommandBuffer command_buffer;
	VkSemaphore image_available_semaphore;
	VkFence render_finished_fence;
	AllocatedBuffer staging_buffer;
} FrameData;

#define MAX_FRAMES_IN_FLIGHT 2

struct VulkanContext {
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice physical_device;
	int queue_family_index;
	VkDevice device;
	VkQueue queue;
	VkSwapchainKHR swapchain;
	VkSurfaceFormatKHR swapchain_surface_format;
	uint32_t swapchain_image_count;
	VkImage* swapchain_images;
	VkExtent2D swapchain_extent;
	bool update_swapchain;

	VkSemaphore* render_finished_semaphores;
	size_t current_frame;
	FrameData frames[MAX_FRAMES_IN_FLIGHT];
};

static struct VulkanContext* vk_ctx = NULL;
static GLFWwindowsizefun previous_resize_callback = NULL;

bool init_instance() {
	VkApplicationInfo app_info;
	memset(&app_info, 0, sizeof(VkApplicationInfo));
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "cvk";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "";
	app_info.engineVersion = VK_MAKE_VERSION(0, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_3;
	app_info.pNext = NULL;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	VkInstanceCreateInfo create_info;
	memset(&create_info, 0, sizeof(VkInstanceCreateInfo));
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.enabledExtensionCount = glfwExtensionCount;
	create_info.ppEnabledExtensionNames = glfwExtensions;
#ifndef NDEBUG
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = NULL;
#else
	create_info.enabledLayerCount = 1;
	const char* validationLayerNames[] = { "VK_LAYER_KHRONOS_validation" };
	create_info.ppEnabledLayerNames = validationLayerNames;
#endif
	create_info.flags = 0;
	create_info.pNext = NULL;

	return vkCreateInstance(&create_info, NULL, &vk_ctx->instance) == VK_SUCCESS;
}

bool check_feature_support(VkPhysicalDevice device) {
	VkPhysicalDeviceFeatures2 features2;
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.pNext = NULL;

	vkGetPhysicalDeviceFeatures2(device, &features2);

	VkStructureType* pNext = &features2;

	while (pNext) {
		switch (*pNext)
		{
		case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2:
			VkPhysicalDeviceFeatures2* features2 = (VkPhysicalDeviceFeatures2*)pNext;
			pNext = features2->pNext;
			break;
		case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES:
			VkPhysicalDeviceVulkan11Features* features11 = (VkPhysicalDeviceVulkan11Features*)pNext;
			pNext = features11->pNext;
			break;
		case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES:
			VkPhysicalDeviceVulkan12Features* features12 = (VkPhysicalDeviceVulkan12Features*)pNext;
			pNext = features12->pNext;
			break;
		case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES:
			VkPhysicalDeviceVulkan13Features* features13 = (VkPhysicalDeviceVulkan13Features*)pNext;
			pNext = features13->pNext;
			// we need these
			if(!features13->dynamicRendering||!features13->synchronization2) {
				return false;
			}
			break;
		/*case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES:
			VkPhysicalDeviceVulkan14Features* features14 = (VkPhysicalDeviceVulkan14Features*)pNext;
			pNext = features14->pNext;
			break;*/
		default:
			assert(0);
		}
	}

	return true;
}

int find_queue_family(VkPhysicalDevice device) {
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

	VkQueueFamilyProperties* queueFamilies = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
	if (!queueFamilies) {
		return -1;
	}

	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

	int i = 0;
	for (; i < queueFamilyCount; i++) {
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vk_ctx->surface, &presentSupport);

		if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
			free(queueFamilies);
			return i;
		}
	}
	free(queueFamilies);
	return -1;
}

bool query_swapchain_support(VkPhysicalDevice device) {
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, vk_ctx->surface, &formatCount, NULL);

	if(formatCount == 0) {
		return false;
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, vk_ctx->surface, &presentModeCount, NULL);

	if(presentModeCount == 0) {
		return false;
	}

	return true;
}

bool is_device_suitable(VkPhysicalDevice device) {
	VkPhysicalDeviceProperties device_properties;
	vkGetPhysicalDeviceProperties(device, &device_properties);

	if (!check_feature_support(device)) {
		return false;
	}

	int queueFamilyIndex = find_queue_family(device);

	if (queueFamilyIndex == -1) {
		return false;
	}

	if (!query_swapchain_support(device)) {
		return false;
	}

	return true;
}

bool select_physical_device() {
	uint32_t deviceCount = 0;
	if(vkEnumeratePhysicalDevices(vk_ctx->instance, &deviceCount, NULL) != VK_SUCCESS) return false;
	if(deviceCount == 0) {
		return false;
	}

	VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
	if(devices == NULL) {
		return false;
	}

	if(vkEnumeratePhysicalDevices(vk_ctx->instance, &deviceCount, devices) != VK_SUCCESS) {
		free(devices);
		return false;
	};

	for (size_t i = 0; i < deviceCount; i++) {
		if(is_device_suitable(devices[i])) {
			vk_ctx->physical_device = devices[i];
			printf("Selected device Nr. %ld\n", i);
			break;
		}
	}

	free(devices);

	if (vk_ctx->physical_device == VK_NULL_HANDLE) {
		return false;
	}

	vk_ctx->queue_family_index = find_queue_family(vk_ctx->physical_device);

	return true;
}

bool create_device() {
	VkDeviceQueueCreateInfo queue_create_info;
	memset(&queue_create_info, 0, sizeof(VkDeviceQueueCreateInfo));
	queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.pNext = NULL;
	queue_create_info.flags = 0;
	queue_create_info.queueFamilyIndex = vk_ctx->queue_family_index;
	queue_create_info.queueCount = 1;
	float queuePriority = 1.0f;
	queue_create_info.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceVulkan13Features features13;
	memset(&features13, 0, sizeof(VkPhysicalDeviceVulkan13Features));
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.pNext = NULL;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	VkPhysicalDeviceFeatures2 deviceFeatures2;
	memset(&deviceFeatures2, 0, sizeof(VkPhysicalDeviceFeatures2));
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &features13;

	const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo create_info;
	memset(&create_info, 0, sizeof(VkDeviceCreateInfo));
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = &deviceFeatures2;
	create_info.flags = 0;
	create_info.queueCreateInfoCount = 1;
	create_info.pQueueCreateInfos = &queue_create_info;
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = NULL;
	create_info.enabledExtensionCount = 1;
	create_info.ppEnabledExtensionNames = &device_extensions;
	create_info.pEnabledFeatures = NULL;

	if (vkCreateDevice(vk_ctx->physical_device, &create_info, NULL, &vk_ctx->device) != VK_SUCCESS) {
		return false;
	}

	return true;
}

void choose_swapchain_surface_format() {
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(vk_ctx->physical_device, vk_ctx->surface, &formatCount, NULL);

	VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
	if (!formats) {
		return;
	}

	vkGetPhysicalDeviceSurfaceFormatsKHR(vk_ctx->physical_device, vk_ctx->surface, &formatCount, formats);

	for (size_t i = 0; i < formatCount; i++) {
		if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			vk_ctx->swapchain_surface_format = formats[i];
			free(formats);
			return;
		}
	}
	vk_ctx->swapchain_surface_format = formats[0];
	free(formats);
}

bool create_swapchain(int width, int height) {
	
	vkDeviceWaitIdle(vk_ctx->device);

	VkSurfaceCapabilitiesKHR swapchain_capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_ctx->physical_device, vk_ctx->surface, &swapchain_capabilities);

	vk_ctx->swapchain_image_count = swapchain_capabilities.minImageCount + 2;

	if (swapchain_capabilities.minImageCount > 0 && vk_ctx->swapchain_image_count > swapchain_capabilities.maxImageCount) {
		vk_ctx->swapchain_image_count = swapchain_capabilities.maxImageCount;
	}

	VkExtent2D desired_extent = {width, height};

	if (desired_extent.width < swapchain_capabilities.minImageExtent.width) {
		desired_extent.width = swapchain_capabilities.minImageExtent.width;
	} else if(desired_extent.width > swapchain_capabilities.maxImageExtent.width) {
		desired_extent.width = swapchain_capabilities.maxImageExtent.width;
	}

	if (desired_extent.height < swapchain_capabilities.minImageExtent.height) {
		desired_extent.height = swapchain_capabilities.minImageExtent.height;
	} else if(desired_extent.height > swapchain_capabilities.maxImageExtent.height) {
		desired_extent.height = swapchain_capabilities.maxImageExtent.height;
	}

	VkSwapchainCreateInfoKHR createInfo;
	memset(&createInfo, 0, sizeof(VkSwapchainCreateInfoKHR));
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	createInfo.surface = vk_ctx->surface;
	createInfo.minImageCount = vk_ctx->swapchain_image_count;
	createInfo.imageFormat = vk_ctx->swapchain_surface_format.format;
	createInfo.imageColorSpace = vk_ctx->swapchain_surface_format.colorSpace;
	createInfo.imageExtent = desired_extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = NULL;
	createInfo.preTransform = swapchain_capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = vk_ctx->swapchain;

	if (vkCreateSwapchainKHR(vk_ctx->device, &createInfo, NULL, &vk_ctx->swapchain) != VK_SUCCESS) {
		return false;
	}

	if (createInfo.oldSwapchain) {
		vkDestroySwapchainKHR(vk_ctx->device, createInfo.oldSwapchain, NULL);
		if (vk_ctx->swapchain_images) {
			free(vk_ctx->swapchain_images);
			vk_ctx->swapchain_images = NULL;
		}
	}

	if (vkGetSwapchainImagesKHR(vk_ctx->device, vk_ctx->swapchain, &vk_ctx->swapchain_image_count, NULL) != VK_SUCCESS) {
		return false;
	};

	vk_ctx->swapchain_images = malloc(sizeof(VkImage) * vk_ctx->swapchain_image_count);
	if (!vk_ctx->swapchain_images) {
		return false;
	}
	memset(vk_ctx->swapchain_images, 0, sizeof(VkImage) * vk_ctx->swapchain_image_count);

	if (vkGetSwapchainImagesKHR(vk_ctx->device, vk_ctx->swapchain, &vk_ctx->swapchain_image_count, vk_ctx->swapchain_images) != VK_SUCCESS) {
		return false;
	};

	return true;
}

void destroy_swapchain() {
	if (vk_ctx->swapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(vk_ctx->device, vk_ctx->swapchain, NULL);
		vk_ctx->swapchain = VK_NULL_HANDLE;
	}

	if (vk_ctx->swapchain_images) {
		free(vk_ctx->swapchain_images);
		vk_ctx->swapchain_images = NULL;
	}
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vk_ctx->physical_device, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	return -1;
}

void destroy_buffer(AllocatedBuffer buffer) {
	if (buffer.memory != VK_NULL_HANDLE) {
		vkFreeMemory(vk_ctx->device, buffer.memory, NULL);
		buffer.memory = VK_NULL_HANDLE;
	}

	if (buffer.buffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(vk_ctx->device, buffer.buffer, NULL);
		buffer.buffer = VK_NULL_HANDLE;
	}
}

AllocatedBuffer create_buffer(size_t size) {
	VkBufferCreateInfo bufferInfo;
	memset(&bufferInfo, 0, sizeof(VkBufferCreateInfo));
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.queueFamilyIndexCount = 0;
	bufferInfo.pQueueFamilyIndices = NULL;

	AllocatedBuffer buffer;
	memset(&buffer, 0, sizeof(AllocatedBuffer));

	if (vkCreateBuffer(vk_ctx->device, &bufferInfo, NULL, &buffer.buffer) != VK_SUCCESS) {
		destroy_buffer(buffer);
		return buffer;
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(vk_ctx->device, buffer.buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo;
	memset(&allocInfo, 0, sizeof(VkMemoryAllocateInfo));
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	if(vkAllocateMemory(vk_ctx->device, &allocInfo, NULL, &buffer.memory) != VK_SUCCESS) {
		destroy_buffer(buffer);
		return buffer;
	}

	vkBindBufferMemory(vk_ctx->device, buffer.buffer, buffer.memory, 0);

	return buffer;
}

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkImageMemoryBarrier barrier;
	memset(&barrier, 0, sizeof(VkImageMemoryBarrier));
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	} else { // TODO: create specific barriers for other transitions if needed
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	}
	vkCmdPipelineBarrier(
		cmd,
		sourceStage, destinationStage,
		0,
		0, NULL,
		0, NULL,
		1, &barrier
	);
}

bool init_swapchain(GLFWwindow* window) {
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	choose_swapchain_surface_format();

	if (!create_swapchain(width, height)) {
		return false;
	}

	vk_ctx->swapchain_extent.width = width;
	vk_ctx->swapchain_extent.height = height;

	return true;
}

bool init_commands() {
	VkCommandPoolCreateInfo commandPoolInfo;
	memset(&commandPoolInfo, 0, sizeof(VkCommandPoolCreateInfo));
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = NULL;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = vk_ctx->queue_family_index;

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if (vkCreateCommandPool(vk_ctx->device, &commandPoolInfo, NULL, &vk_ctx->frames[i].command_pool) != VK_SUCCESS) {
			return false;
		}

		VkCommandBufferAllocateInfo commandBufferAllocInfo;
		memset(&commandBufferAllocInfo, 0, sizeof(VkCommandBufferAllocateInfo));
		commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocInfo.pNext = NULL;
		commandBufferAllocInfo.commandPool = vk_ctx->frames[i].command_pool;
		commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(vk_ctx->device, &commandBufferAllocInfo, &vk_ctx->frames[i].command_buffer) != VK_SUCCESS) {
			return false;
		}

		VkSemaphoreCreateInfo semaphoreInfo;
		memset(&semaphoreInfo, 0, sizeof(VkSemaphoreCreateInfo));
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreInfo.pNext = NULL;

		if (vkCreateSemaphore(vk_ctx->device, &semaphoreInfo, NULL, &vk_ctx->frames[i].image_available_semaphore) != VK_SUCCESS) {
			return false;
		}

		VkFenceCreateInfo fenceInfo;
		memset(&fenceInfo, 0, sizeof(VkFenceCreateInfo));
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.pNext = NULL;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateFence(vk_ctx->device, &fenceInfo, NULL, &vk_ctx->frames[i].render_finished_fence) != VK_SUCCESS) {
			return false;
		}
	}

	vk_ctx->render_finished_semaphores = malloc(sizeof(VkSemaphore) * vk_ctx->swapchain_image_count);
	if (!vk_ctx->render_finished_semaphores) {
		return false;
	}

	for (int i = 0; i < vk_ctx->swapchain_image_count; i++) {
		VkSemaphoreCreateInfo semaphoreInfo;
		memset(&semaphoreInfo, 0, sizeof(VkSemaphoreCreateInfo));
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreInfo.pNext = NULL;

		if (vkCreateSemaphore(vk_ctx->device, &semaphoreInfo, NULL, &vk_ctx->render_finished_semaphores[i]) != VK_SUCCESS) {
			return false;
		}
	}

	return true;
}

bool create_buffers() {
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk_ctx->frames[i].staging_buffer = create_buffer(vk_ctx->swapchain_extent.width * vk_ctx->swapchain_extent.height * 4);
		if (vk_ctx->frames[i].staging_buffer.buffer == VK_NULL_HANDLE) {
			return false;
		}
		if (vk_ctx->frames[i].staging_buffer.memory == VK_NULL_HANDLE) {
			return false;
		}
	}

	return true;
}

void cvk_window_resize_callback(GLFWwindow* window, int width, int height) {
	if(vk_ctx) vk_ctx->update_swapchain = true;

	if(previous_resize_callback) {
		previous_resize_callback(window, width, height);
	}
}

bool cvk_init(GLFWwindow* window) {
	vk_ctx = malloc(sizeof(struct VulkanContext));

	if (vk_ctx == NULL) {
		return false;
	}

	memset(vk_ctx, 0, sizeof(struct VulkanContext));

	if(!init_instance()) {
		cvk_cleanup();
		return false;
	}

	if(glfwCreateWindowSurface(vk_ctx->instance, window, NULL, &vk_ctx->surface) != VK_SUCCESS) {
		cvk_cleanup();
		return false;
	}

	previous_resize_callback = glfwSetWindowSizeCallback(window, cvk_window_resize_callback);

	if(!select_physical_device()) {
		cvk_cleanup();
		return false;
	}

	if(!create_device()) {
		cvk_cleanup();
		return false;
	}

	vkGetDeviceQueue(vk_ctx->device, vk_ctx->queue_family_index, 0, &vk_ctx->queue);

	if(!init_swapchain(window)) {
		cvk_cleanup();
		return false;
	}

	if(!init_commands()) {
		cvk_cleanup();
		return false;
	}

	if (!create_buffers()) {
		cvk_cleanup();
		return false;
	}

	return true;
}

void cvk_cleanup() {
	if(!vk_ctx) {
		return;
	}

	if(vk_ctx->device)
		vkDeviceWaitIdle(vk_ctx->device);

	if(vk_ctx->render_finished_semaphores) {
		for (int i = 0; i < vk_ctx->swapchain_image_count; i++) {
			if (vk_ctx->render_finished_semaphores[i]) {
				vkDestroySemaphore(vk_ctx->device, vk_ctx->render_finished_semaphores[i], NULL);
			}
		}

		free(vk_ctx->render_finished_semaphores);
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		if(vk_ctx->frames[i].command_pool) {
			vkDestroyCommandPool(vk_ctx->device, vk_ctx->frames[i].command_pool, NULL);
		}
		if(vk_ctx->frames[i].image_available_semaphore) {
			vkDestroySemaphore(vk_ctx->device, vk_ctx->frames[i].image_available_semaphore, NULL);
		}
		if(vk_ctx->frames[i].render_finished_fence) {
			vkDestroyFence(vk_ctx->device, vk_ctx->frames[i].render_finished_fence, NULL);
		}
		destroy_buffer(vk_ctx->frames[i].staging_buffer);

		vk_ctx->frames[i].command_pool = VK_NULL_HANDLE;
		vk_ctx->frames[i].image_available_semaphore = VK_NULL_HANDLE;
		vk_ctx->frames[i].render_finished_fence = VK_NULL_HANDLE;
	}

	destroy_swapchain();

	if(vk_ctx->device) {
		vkDestroyDevice(vk_ctx->device, NULL);
	}

	if(vk_ctx->surface) {
		vkDestroySurfaceKHR(vk_ctx->instance, vk_ctx->surface, NULL);
	}

	if(vk_ctx->instance) {
		vkDestroyInstance(vk_ctx->instance, NULL);
	}

	free(vk_ctx);
	vk_ctx = NULL;
}

void cvk_draw(GLFWwindow* window, void* image, size_t width, size_t height) {
	if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) return;

	if (vk_ctx->update_swapchain) {
		vkDeviceWaitIdle(vk_ctx->device);

		vk_ctx->update_swapchain = false;
		assert(init_swapchain(window));
		printf("Swapchain recreated with new size %dx%d\n", vk_ctx->swapchain_extent.width, vk_ctx->swapchain_extent.height);
	}

	FrameData* frame = &vk_ctx->frames[vk_ctx->current_frame%MAX_FRAMES_IN_FLIGHT];

	vkWaitForFences(vk_ctx->device, 1, &frame->render_finished_fence, VK_TRUE, UINT64_MAX);

	VkExtent3D copyExtent;
	copyExtent.width = vk_ctx->swapchain_extent.width;
	copyExtent.height = vk_ctx->swapchain_extent.height;
	copyExtent.depth = 1;

	if (width < copyExtent.width) {
		copyExtent.width = width;
	}
	if (height < copyExtent.height) {
		copyExtent.height = height;
	}

	{
		void* data;
		vkMapMemory(vk_ctx->device, frame->staging_buffer.memory, 0, VK_WHOLE_SIZE, 0, &data);
		memcpy(data, image, copyExtent.width * copyExtent.height * 4);
		vkUnmapMemory(vk_ctx->device, frame->staging_buffer.memory);
	}

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(vk_ctx->device, vk_ctx->swapchain, UINT64_MAX, frame->image_available_semaphore, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		vk_ctx->update_swapchain = true;
		return;
	}

	if (result == VK_SUBOPTIMAL_KHR) {
		vk_ctx->update_swapchain = true;
	} else if (result != VK_SUCCESS) {
		assert(0);
	}

	vkResetFences(vk_ctx->device, 1, &frame->render_finished_fence);

	VkCommandBuffer cmd = frame->command_buffer;

	vkResetCommandBuffer(cmd, 0);

	VkCommandBufferBeginInfo cmdBeginInfo;
	memset(&cmdBeginInfo, 0, sizeof(VkCommandBufferBeginInfo));
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = NULL;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(cmd, &cmdBeginInfo);

	transition_image(cmd, vk_ctx->swapchain_images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	VkClearColorValue clearValue = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkImageSubresourceRange clearRange;
	memset(&clearRange, 0, sizeof(VkImageSubresourceRange));
	clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearRange.baseMipLevel = 0;
	clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
	clearRange.baseArrayLayer = 0;
	clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	vkCmdClearColorImage(cmd, vk_ctx->swapchain_images[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

	transition_image(cmd, vk_ctx->swapchain_images[imageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkBufferImageCopy region;
	memset(&region, 0, sizeof(VkBufferImageCopy));
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = copyExtent;
	region.bufferRowLength = width;
	region.bufferImageHeight = height;

	vkCmdCopyBufferToImage(cmd, frame->staging_buffer.buffer, vk_ctx->swapchain_images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	transition_image(cmd, vk_ctx->swapchain_images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	vkEndCommandBuffer(cmd);

	VkCommandBufferSubmitInfo submitInfo;
	memset(&submitInfo, 0, sizeof(VkCommandBufferSubmitInfo));
	submitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.commandBuffer = cmd;

	VkSemaphoreSubmitInfo waitSemaphoreInfo;
	memset(&waitSemaphoreInfo, 0, sizeof(VkSemaphoreSubmitInfo));
	waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSemaphoreInfo.pNext = NULL;
	waitSemaphoreInfo.semaphore = frame->image_available_semaphore;
	waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signalSemaphoreInfo;
	memset(&signalSemaphoreInfo, 0, sizeof(VkSemaphoreSubmitInfo));
	signalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphoreInfo.pNext = NULL;
	signalSemaphoreInfo.semaphore = vk_ctx->render_finished_semaphores[imageIndex];
	signalSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

	VkSubmitInfo2 submitInfo2;
	memset(&submitInfo2, 0, sizeof(VkSubmitInfo2));
	submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo2.pNext = NULL;
	submitInfo2.flags = 0;
	submitInfo2.waitSemaphoreInfoCount = 1;
	submitInfo2.pWaitSemaphoreInfos = &waitSemaphoreInfo;
	submitInfo2.commandBufferInfoCount = 1;
	submitInfo2.pCommandBufferInfos = &submitInfo;
	submitInfo2.signalSemaphoreInfoCount = 1;
	submitInfo2.pSignalSemaphoreInfos = &signalSemaphoreInfo;

	if (vkQueueSubmit2(vk_ctx->queue, 1, &submitInfo2, frame->render_finished_fence) != VK_SUCCESS) {
		assert(0);
	}

	VkPresentInfoKHR presentInfo;
	memset(&presentInfo, 0, sizeof(VkPresentInfoKHR));
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &vk_ctx->render_finished_semaphores[imageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &vk_ctx->swapchain;
	presentInfo.pImageIndices = &imageIndex;

	VkResult presentResult = vkQueuePresentKHR(vk_ctx->queue, &presentInfo);
	assert(presentResult == VK_SUCCESS || presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR);
	if(presentResult == VK_ERROR_OUT_OF_DATE_KHR) vk_ctx->update_swapchain = true;

	vk_ctx->current_frame++;
}
