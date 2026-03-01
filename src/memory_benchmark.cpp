#include "memory_benchmark.h"
#include "logger.h"
#include "progress.h"
#include <algorithm>
#include <cstring>

MemoryBenchmark::MemoryBenchmark(VulkanContext& ctx, const BenchmarkConfig& config)
    : ctx_(ctx), config_(config) {}
MemoryBenchmark::~MemoryBenchmark() = default;

void MemoryBenchmark::cleanup() {}

double MemoryBenchmark::measureHostToDevice(VkDeviceSize size, int numIterations) {
    VkDevice device = ctx_.getDevice();

    // Create staging buffer (host visible)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

        // Fill with data
        void* data;
        vkMapMemory(device, stagingMemory, 0, size, 0, &data);
        memset(data, 0xAB, size);
        vkUnmapMemory(device, stagingMemory);
    }

    // Create device-local buffer
    VkBuffer deviceBuffer;
    VkDeviceMemory deviceMemory;
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &deviceBuffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, deviceBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory);
        vkBindBufferMemory(device, deviceBuffer, deviceMemory, 0);
    }

    VkCommandPool pool = ctx_.getComputeCommandPool();
    if (pool == VK_NULL_HANDLE) pool = ctx_.getGraphicsCommandPool();

    VkQueue queue = ctx_.getComputeQueue();
    if (queue == VK_NULL_HANDLE) queue = ctx_.getGraphicsQueue();

    // Warmup
    {
        auto cmd = ctx_.beginSingleTimeCommands(pool);
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, stagingBuffer, deviceBuffer, 1, &copyRegion);
        ctx_.endSingleTimeCommands(pool, queue, cmd);
    }

    // Timed runs
    double bestBandwidth = 0.0;

    for (int i = 0; i < numIterations; i++) {
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, stagingBuffer, deviceBuffer, 1, &copyRegion);

        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence;
        vkCreateFence(device, &fenceInfo, nullptr, &fence);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        auto start = Clock::now();
        vkQueueSubmit(queue, 1, &submitInfo, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        auto end = Clock::now();

        double seconds = elapsedSec(start, end);
        double bandwidth = (static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0)) / seconds;
        bestBandwidth = std::max(bestBandwidth, bandwidth);

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyBuffer(device, deviceBuffer, nullptr);
    vkFreeMemory(device, deviceMemory, nullptr);

    return bestBandwidth;
}

double MemoryBenchmark::measureDeviceToHost(VkDeviceSize size, int numIterations) {
    VkDevice device = ctx_.getDevice();

    // Create device-local buffer (source)
    VkBuffer deviceBuffer;
    VkDeviceMemory deviceMemory;
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &deviceBuffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, deviceBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory);
        vkBindBufferMemory(device, deviceBuffer, deviceMemory, 0);
    }

    // Create staging buffer (host visible, destination)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory);
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    }

    VkCommandPool pool = ctx_.getComputeCommandPool();
    if (pool == VK_NULL_HANDLE) pool = ctx_.getGraphicsCommandPool();

    VkQueue queue = ctx_.getComputeQueue();
    if (queue == VK_NULL_HANDLE) queue = ctx_.getGraphicsQueue();

    // Warmup
    {
        auto cmd = ctx_.beginSingleTimeCommands(pool);
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, deviceBuffer, stagingBuffer, 1, &copyRegion);
        ctx_.endSingleTimeCommands(pool, queue, cmd);
    }

    // Timed runs
    double bestBandwidth = 0.0;

    for (int i = 0; i < numIterations; i++) {
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, deviceBuffer, stagingBuffer, 1, &copyRegion);

        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence;
        vkCreateFence(device, &fenceInfo, nullptr, &fence);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        auto start = Clock::now();
        vkQueueSubmit(queue, 1, &submitInfo, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        auto end = Clock::now();

        double seconds = elapsedSec(start, end);
        double bandwidth = (static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0)) / seconds;
        bestBandwidth = std::max(bestBandwidth, bandwidth);

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyBuffer(device, deviceBuffer, nullptr);
    vkFreeMemory(device, deviceMemory, nullptr);

    return bestBandwidth;
}

double MemoryBenchmark::measureDeviceToDevice(VkDeviceSize size, int numIterations) {
    VkDevice device = ctx_.getDevice();

    auto createDeviceLocalBuffer = [&](VkBufferUsageFlags usage) -> std::pair<VkBuffer, VkDeviceMemory> {
        VkBuffer buffer;
        VkDeviceMemory memory;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &memory);
        vkBindBufferMemory(device, buffer, memory, 0);

        return {buffer, memory};
    };

    auto [srcBuffer, srcMemory] = createDeviceLocalBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto [dstBuffer, dstMemory] = createDeviceLocalBuffer(
        VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    VkCommandPool pool = ctx_.getComputeCommandPool();
    if (pool == VK_NULL_HANDLE) pool = ctx_.getGraphicsCommandPool();

    VkQueue queue = ctx_.getComputeQueue();
    if (queue == VK_NULL_HANDLE) queue = ctx_.getGraphicsQueue();

    // Initialize source buffer to commit memory pages
    {
        auto cmd = ctx_.beginSingleTimeCommands(pool);
        vkCmdFillBuffer(cmd, srcBuffer, 0, size, 0xABCDABCD);
        ctx_.endSingleTimeCommands(pool, queue, cmd);
    }

    // Warmup
    {
        auto cmd = ctx_.beginSingleTimeCommands(pool);
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);
        ctx_.endSingleTimeCommands(pool, queue, cmd);
    }

    // Timed runs
    double bestBandwidth = 0.0;

    for (int i = 0; i < numIterations; i++) {
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = pool;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence;
        vkCreateFence(device, &fenceInfo, nullptr, &fence);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        auto start = Clock::now();
        vkQueueSubmit(queue, 1, &submitInfo, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        auto end = Clock::now();

        double seconds = elapsedSec(start, end);
        double bandwidth = (static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0)) / seconds;
        bestBandwidth = std::max(bestBandwidth, bandwidth);

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, pool, 1, &cmd);
    }

    vkDestroyBuffer(device, srcBuffer, nullptr);
    vkFreeMemory(device, srcMemory, nullptr);
    vkDestroyBuffer(device, dstBuffer, nullptr);
    vkFreeMemory(device, dstMemory, nullptr);

    return bestBandwidth;
}

std::vector<BenchmarkResult> MemoryBenchmark::run() {
    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(config_.memoryBufferSizeMB) * 1024 * 1024;
    int iterations = static_cast<int>(config_.memoryIterations);
    bool showProgress = config_.showProgress && Logger::instance().isNormal();

    LOG_DETAIL("Memory config: buffer=" + std::to_string(config_.memoryBufferSizeMB) +
               "MB iterations=" + std::to_string(iterations));

    ProgressBar progress("Memory", 3, showProgress);

    double h2d = measureHostToDevice(bufferSize, iterations);
    progress.increment();

    double d2h = measureDeviceToHost(bufferSize, iterations);
    progress.increment();

    double d2d = measureDeviceToDevice(bufferSize, iterations);
    progress.increment();

    std::string desc = std::to_string(config_.memoryBufferSizeMB) + " MB, " +
                       BenchmarkConfig::modeToString(config_.mode) + " mode";

    LOG_BENCH("Host->Device", h2d, "GB/s");
    LOG_BENCH("Device->Host", d2h, "GB/s");
    LOG_BENCH("VRAM Bandwidth", d2d, "GB/s");

    progress.finish(std::to_string(static_cast<int>(h2d)) + "/" +
                    std::to_string(static_cast<int>(d2h)) + "/" +
                    std::to_string(static_cast<int>(d2d)) + " GB/s");

    return {
        {"Host -> Device", "GB/s", h2d, "Host to device transfer (" + desc + ")"},
        {"Device -> Host", "GB/s", d2h, "Device to host transfer (" + desc + ")"},
        {"VRAM Bandwidth", "GB/s", d2d, "Device-local memory bandwidth (" + desc + ")"}
    };
}
