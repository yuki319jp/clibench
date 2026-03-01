#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"
#include "vulkan_context.h"

class ComputeBenchmark : public BenchmarkBase {
public:
    ComputeBenchmark(VulkanContext& ctx, const BenchmarkConfig& config);
    ~ComputeBenchmark();

    std::string getName() const override { return "Compute (GFLOPS)"; }
    std::vector<BenchmarkResult> run() override;

private:
    void createPipeline();
    void createBuffers();
    void cleanup();

    VulkanContext& ctx_;
    BenchmarkConfig config_;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet_ = VK_NULL_HANDLE;
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VkDeviceMemory bufferMemory_ = VK_NULL_HANDLE;

    static constexpr uint32_t WORKGROUP_SIZE = 256;
};
