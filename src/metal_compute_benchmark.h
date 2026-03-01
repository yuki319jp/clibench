#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"

class MetalContext;

class MetalComputeBenchmark : public BenchmarkBase {
public:
    MetalComputeBenchmark(MetalContext& ctx, const BenchmarkConfig& config);
    ~MetalComputeBenchmark();

    std::string getName() const override { return "Compute (GFLOPS)"; }
    std::vector<BenchmarkResult> run() override;

private:
    MetalContext& ctx_;
    BenchmarkConfig config_;
};
