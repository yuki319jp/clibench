#pragma once

#include <vulkan/vulkan.h>
#include "gpu_info.h"
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <iostream>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> transferFamily;

    bool hasGraphics() const { return graphicsFamily.has_value(); }
    bool hasCompute() const { return computeFamily.has_value(); }
    bool hasTransfer() const { return transferFamily.has_value(); }
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    void init(int gpuIndex = -1);
    void cleanup();

    GPUInfo getGPUInfo() const;
    std::vector<std::string> listGPUs() const;

    VkInstance getInstance() const { return instance_; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice_; }
    VkDevice getDevice() const { return device_; }
    VkQueue getComputeQueue() const { return computeQueue_; }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    VkCommandPool getComputeCommandPool() const { return computeCommandPool_; }
    VkCommandPool getGraphicsCommandPool() const { return graphicsCommandPool_; }
    const QueueFamilyIndices& getQueueFamilyIndices() const { return queueFamilyIndices_; }
    const VkPhysicalDeviceProperties& getDeviceProperties() const { return deviceProperties_; }
    const VkPhysicalDeviceMemoryProperties& getMemoryProperties() const { return memoryProperties_; }

    // Vulkan version capabilities
    uint32_t getInstanceApiVersion() const { return instanceApiVersion_; }
    uint32_t getDeviceApiVersion() const { return deviceProperties_.apiVersion; }
    bool supportsVulkan13() const { return VK_VERSION_MINOR(deviceProperties_.apiVersion) >= 3; }
    bool supportsVulkan14() const { return VK_VERSION_MINOR(deviceProperties_.apiVersion) >= 4; }
    bool supportsDynamicRendering() const { return dynamicRenderingSupported_; }
    bool supportsPushDescriptor() const { return pushDescriptorSupported_; }
    bool supportsSynchronization2() const { return synchronization2Supported_; }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool);
    void endSingleTimeCommands(VkCommandPool pool, VkQueue queue, VkCommandBuffer commandBuffer);

private:
    void createInstance();
    void pickPhysicalDevice(int gpuIndex);
    void createLogicalDevice();
    void createCommandPools();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue computeQueue_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkCommandPool computeCommandPool_ = VK_NULL_HANDLE;
    VkCommandPool graphicsCommandPool_ = VK_NULL_HANDLE;

    QueueFamilyIndices queueFamilyIndices_;
    VkPhysicalDeviceProperties deviceProperties_{};
    VkPhysicalDeviceMemoryProperties memoryProperties_{};
    std::vector<VkPhysicalDevice> physicalDevices_;

    uint32_t instanceApiVersion_ = VK_API_VERSION_1_0;
    bool dynamicRenderingSupported_ = false;
    bool pushDescriptorSupported_ = false;
    bool synchronization2Supported_ = false;
};
