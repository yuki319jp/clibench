#include "compute_benchmark.h"
#include "compute_comp.h"
#include "logger.h"
#include "progress.h"
#include <cstring>

ComputeBenchmark::ComputeBenchmark(VulkanContext& ctx, const BenchmarkConfig& config)
    : ctx_(ctx), config_(config) {}

ComputeBenchmark::~ComputeBenchmark() {
    cleanup();
}

void ComputeBenchmark::cleanup() {
    VkDevice device = ctx_.getDevice();
    if (device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(device);

    if (pipeline_ != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipeline_, nullptr);
    if (pipelineLayout_ != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);
    if (descriptorPool_ != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
    if (descriptorSetLayout_ != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout_, nullptr);
    if (buffer_ != VK_NULL_HANDLE)
        vkDestroyBuffer(device, buffer_, nullptr);
    if (bufferMemory_ != VK_NULL_HANDLE)
        vkFreeMemory(device, bufferMemory_, nullptr);

    pipeline_ = VK_NULL_HANDLE;
    pipelineLayout_ = VK_NULL_HANDLE;
    descriptorPool_ = VK_NULL_HANDLE;
    descriptorSetLayout_ = VK_NULL_HANDLE;
    buffer_ = VK_NULL_HANDLE;
    bufferMemory_ = VK_NULL_HANDLE;
}

void ComputeBenchmark::createBuffers() {
    VkDevice device = ctx_.getDevice();
    VkDeviceSize bufferSize = sizeof(float) * 4 * config_.computeElements;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute buffer");
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer_, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx_.findMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate compute buffer memory");
    }

    vkBindBufferMemory(device, buffer_, bufferMemory_, 0);
}

void ComputeBenchmark::createPipeline() {
    VkDevice device = ctx_.getDevice();

    // Descriptor set layout
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout");
    }

    // Push constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(uint32_t);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    // Shader module
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = compute_comp_size;
    shaderInfo.pCode = reinterpret_cast<const uint32_t*>(compute_comp_data);

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create compute shader module");
    }

    // Compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(device, shaderModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline");
    }

    vkDestroyShaderModule(device, shaderModule, nullptr);

    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool");
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout_;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set");
    }

    // Update descriptor set
    VkDescriptorBufferInfo bufferDesc{};
    bufferDesc.buffer = buffer_;
    bufferDesc.offset = 0;
    bufferDesc.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writeDesc{};
    writeDesc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDesc.dstSet = descriptorSet_;
    writeDesc.dstBinding = 0;
    writeDesc.descriptorCount = 1;
    writeDesc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writeDesc.pBufferInfo = &bufferDesc;

    vkUpdateDescriptorSets(device, 1, &writeDesc, 0, nullptr);
}

std::vector<BenchmarkResult> ComputeBenchmark::run() {
    if (!ctx_.getQueueFamilyIndices().hasCompute()) {
        return {{getName(), "GFLOPS", 0.0, "No compute queue available"}};
    }

    uint32_t numElements = config_.computeElements;
    uint32_t iterations = config_.computeIterations;
    uint32_t runs = config_.computeRuns;

    LOG_DETAIL("Compute config: elements=" + std::to_string(numElements) +
               " iterations=" + std::to_string(iterations) + " runs=" + std::to_string(runs));

    createBuffers();
    createPipeline();

    VkDevice device = ctx_.getDevice();
    uint32_t numGroups = numElements / WORKGROUP_SIZE;

    bool showProgress = config_.showProgress && Logger::instance().isNormal();

    // Warmup
    {
        LOG_DEBUG("Warmup dispatch");
        auto cmd = ctx_.beginSingleTimeCommands(ctx_.getComputeCommandPool());
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        uint32_t iters = 16;
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &iters);
        vkCmdDispatch(cmd, numGroups, 1, 1);
        ctx_.endSingleTimeCommands(ctx_.getComputeCommandPool(), ctx_.getComputeQueue(), cmd);
    }

    double bestGflops = 0.0;
    ProgressBar progress("Compute", static_cast<int>(runs), showProgress);

    for (uint32_t r = 0; r < runs; r++) {
        // Timed run with fence timing
        VkCommandBuffer cmd;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = ctx_.getComputeCommandPool();
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 1, &descriptorSet_, 0, nullptr);
        uint32_t iters = iterations;
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &iters);
        vkCmdDispatch(cmd, numGroups, 1, 1);

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
        vkQueueSubmit(ctx_.getComputeQueue(), 1, &submitInfo, fence);
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        auto end = Clock::now();

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, ctx_.getComputeCommandPool(), 1, &cmd);

        double seconds = elapsedSec(start, end);
        double totalFlops = static_cast<double>(numElements) * iterations * 4.0 * 4.0 * 2.0;
        double gflops = (totalFlops / seconds) / 1e9;

        LOG_TIME("Compute run " + std::to_string(r + 1), elapsedMs(start, end));
        LOG_BENCH("Run " + std::to_string(r + 1), gflops, "GFLOPS");

        if (gflops > bestGflops) bestGflops = gflops;

        progress.increment();
    }

    progress.finish(std::to_string(static_cast<int>(bestGflops)) + " GFLOPS");

    cleanup();

    return {
        {getName(), "GFLOPS", bestGflops, "FMA compute throughput (" +
         BenchmarkConfig::modeToString(config_.mode) + " mode)"}
    };
}
