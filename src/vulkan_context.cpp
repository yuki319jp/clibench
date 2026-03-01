#include "vulkan_context.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstring>
#include <set>
#include <algorithm>

static std::string vkVersionToString(uint32_t version) {
    return std::to_string(VK_VERSION_MAJOR(version)) + "." +
           std::to_string(VK_VERSION_MINOR(version)) + "." +
           std::to_string(VK_VERSION_PATCH(version));
}

static std::string deviceTypeToString(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete GPU";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
        default:                                     return "Unknown";
    }
}

VulkanContext::VulkanContext() = default;

VulkanContext::~VulkanContext() {
    cleanup();
}

void VulkanContext::init(int gpuIndex) {
    createInstance();
    pickPhysicalDevice(gpuIndex);
    createLogicalDevice();
    createCommandPools();
}

void VulkanContext::cleanup() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        if (computeCommandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, computeCommandPool_, nullptr);
            computeCommandPool_ = VK_NULL_HANDLE;
        }
        if (graphicsCommandPool_ != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device_, graphicsCommandPool_, nullptr);
            graphicsCommandPool_ = VK_NULL_HANDLE;
        }
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::createInstance() {
    // Query the highest Vulkan API version supported by the loader
    instanceApiVersion_ = VK_API_VERSION_1_0;
    auto pfnEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    if (pfnEnumerateInstanceVersion) {
        pfnEnumerateInstanceVersion(&instanceApiVersion_);
    }

    // Request up to Vulkan 1.4, capped by what the loader supports
    uint32_t requestedVersion = std::min(instanceApiVersion_, static_cast<uint32_t>(VK_API_VERSION_1_4));

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "CLIBench";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "CLIBench";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = requestedVersion;

    // Get required extensions (GLFW provides surface extensions)
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = nullptr;

    // Get GLFW extensions if GLFW is initialized
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions;
    if (glfwExtensions) {
        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            extensions.push_back(glfwExtensions[i]);
        }
    }

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    // VK_KHR_get_physical_device_properties2 is core since Vulkan 1.1,
    // but add it for MoltenVK compatibility when instance version < 1.1
    if (VK_VERSION_MINOR(requestedVersion) < 1) {
        extensions.push_back("VK_KHR_get_physical_device_properties2");
    }
#endif

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance (error: " + std::to_string(result) + ")");
    }
}

void VulkanContext::pickPhysicalDevice(int gpuIndex) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable GPU found");
    }

    physicalDevices_.resize(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, physicalDevices_.data());

    if (gpuIndex >= 0) {
        if (gpuIndex >= static_cast<int>(deviceCount)) {
            throw std::runtime_error("GPU index " + std::to_string(gpuIndex) +
                                     " out of range (found " + std::to_string(deviceCount) + " devices)");
        }
        physicalDevice_ = physicalDevices_[gpuIndex];
    } else {
        // Prefer discrete GPU
        physicalDevice_ = physicalDevices_[0];
        for (auto& dev : physicalDevices_) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physicalDevice_ = dev;
                break;
            }
        }
    }

    vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties_);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties_);
    queueFamilyIndices_ = findQueueFamilies(physicalDevice_);
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            indices.computeFamily = i;
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
            indices.transferFamily = i;
        }
    }

    return indices;
}

void VulkanContext::createLogicalDevice() {
    std::set<uint32_t> uniqueQueueFamilies;
    if (queueFamilyIndices_.hasCompute())
        uniqueQueueFamilies.insert(queueFamilyIndices_.computeFamily.value());
    if (queueFamilyIndices_.hasGraphics())
        uniqueQueueFamilies.insert(queueFamilyIndices_.graphicsFamily.value());

    if (uniqueQueueFamilies.empty()) {
        throw std::runtime_error("No suitable queue families found");
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    float queuePriority = 1.0f;

    for (uint32_t family : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#ifdef __APPLE__
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

    uint32_t deviceVersion = deviceProperties_.apiVersion;

    // Build feature chain based on device capabilities
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceVulkan14Features features14{};
    features14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;

    // Chain feature structs based on device version
    void** ppNext = &features2.pNext;
    if (VK_VERSION_MINOR(deviceVersion) >= 2) {
        *ppNext = &features12;
        ppNext = &features12.pNext;
    }
    if (VK_VERSION_MINOR(deviceVersion) >= 3) {
        *ppNext = &features13;
        ppNext = &features13.pNext;
    }
    if (VK_VERSION_MINOR(deviceVersion) >= 4) {
        *ppNext = &features14;
        ppNext = &features14.pNext;
    }

    // Query supported features
    vkGetPhysicalDeviceFeatures2(physicalDevice_, &features2);

    // Record which features are available
    if (VK_VERSION_MINOR(deviceVersion) >= 3) {
        dynamicRenderingSupported_ = features13.dynamicRendering == VK_TRUE;
        synchronization2Supported_ = features13.synchronization2 == VK_TRUE;
    }
    if (VK_VERSION_MINOR(deviceVersion) >= 4) {
        pushDescriptorSupported_ = features14.pushDescriptor == VK_TRUE;
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = nullptr; // Using pNext chain instead
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pNext = &features2;

    VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device (error: " + std::to_string(result) + ")");
    }

    if (queueFamilyIndices_.hasCompute())
        vkGetDeviceQueue(device_, queueFamilyIndices_.computeFamily.value(), 0, &computeQueue_);
    if (queueFamilyIndices_.hasGraphics())
        vkGetDeviceQueue(device_, queueFamilyIndices_.graphicsFamily.value(), 0, &graphicsQueue_);
}

void VulkanContext::createCommandPools() {
    if (queueFamilyIndices_.hasCompute()) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices_.computeFamily.value();

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &computeCommandPool_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute command pool");
        }
    }

    if (queueFamilyIndices_.hasGraphics()) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices_.graphicsFamily.value();

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &graphicsCommandPool_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics command pool");
        }
    }
}

GPUInfo VulkanContext::getGPUInfo() const {
    GPUInfo info;
    info.name = deviceProperties_.deviceName;
    info.backend = "Vulkan";
    info.driverVersion = vkVersionToString(deviceProperties_.driverVersion);
    info.apiVersion = vkVersionToString(deviceProperties_.apiVersion);
    info.deviceType = deviceTypeToString(deviceProperties_.deviceType);

    // Include Vulkan 1.3/1.4 feature availability
    if (dynamicRenderingSupported_ || pushDescriptorSupported_) {
        info.apiVersion += " (";
        std::vector<std::string> feats;
        if (dynamicRenderingSupported_) feats.push_back("dynamicRendering");
        if (synchronization2Supported_) feats.push_back("sync2");
        if (pushDescriptorSupported_) feats.push_back("pushDescriptor");
        for (size_t i = 0; i < feats.size(); i++) {
            if (i > 0) info.apiVersion += ", ";
            info.apiVersion += feats[i];
        }
        info.apiVersion += ")";
    }

    info.maxComputeWorkGroupSize[0] = deviceProperties_.limits.maxComputeWorkGroupSize[0];
    info.maxComputeWorkGroupSize[1] = deviceProperties_.limits.maxComputeWorkGroupSize[1];
    info.maxComputeWorkGroupSize[2] = deviceProperties_.limits.maxComputeWorkGroupSize[2];
    info.maxComputeWorkGroupInvocations = deviceProperties_.limits.maxComputeWorkGroupInvocations;

    info.deviceLocalMemoryMB = 0;
    info.hostVisibleMemoryMB = 0;

    for (uint32_t i = 0; i < memoryProperties_.memoryHeapCount; i++) {
        auto& heap = memoryProperties_.memoryHeaps[i];
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            info.deviceLocalMemoryMB += heap.size / (1024 * 1024);
        }
    }

    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; i++) {
        if (memoryProperties_.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            auto heapIdx = memoryProperties_.memoryTypes[i].heapIndex;
            info.hostVisibleMemoryMB = memoryProperties_.memoryHeaps[heapIdx].size / (1024 * 1024);
            break;
        }
    }

    return info;
}

std::vector<std::string> VulkanContext::listGPUs() const {
    std::vector<std::string> gpus;
    for (auto& dev : physicalDevices_) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        gpus.push_back(std::string(props.deviceName) + " (" + deviceTypeToString(props.deviceType) + ")");
    }
    return gpus;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    for (uint32_t i = 0; i < memoryProperties_.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memoryProperties_.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands(VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence;
    vkCreateFence(device_, &fenceInfo, nullptr, &fence);

    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, pool, 1, &commandBuffer);
}
