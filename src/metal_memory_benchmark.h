#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"

class MetalContext;

class MetalMemoryBenchmark : public BenchmarkBase {
public:
    MetalMemoryBenchmark(MetalContext& ctx, const BenchmarkConfig& config);
    ~MetalMemoryBenchmark();

    std::string getName() const override { return "Memory Bandwidth"; }
    std::vector<BenchmarkResult> run() override;

private:
    MetalContext& ctx_;
    BenchmarkConfig config_;
};
