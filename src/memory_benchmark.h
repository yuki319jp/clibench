#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"
#include "vulkan_context.h"

class MemoryBenchmark : public BenchmarkBase {
public:
    MemoryBenchmark(VulkanContext& ctx, const BenchmarkConfig& config);
    ~MemoryBenchmark();

    std::string getName() const override { return "Memory Bandwidth"; }
    std::vector<BenchmarkResult> run() override;

private:
    void cleanup();
    double measureHostToDevice(VkDeviceSize size, int iterations);
    double measureDeviceToHost(VkDeviceSize size, int iterations);
    double measureDeviceToDevice(VkDeviceSize size, int iterations);

    VulkanContext& ctx_;
    BenchmarkConfig config_;
};
