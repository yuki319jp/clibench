#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"

class MetalContext;

class MetalGraphicsBenchmark : public BenchmarkBase {
public:
    MetalGraphicsBenchmark(MetalContext& ctx, const BenchmarkConfig& config);
    ~MetalGraphicsBenchmark();

    std::string getName() const override { return "Graphics"; }
    std::vector<BenchmarkResult> run() override;

private:
    MetalContext& ctx_;
    BenchmarkConfig config_;

    static constexpr uint32_t WIDTH = 1280;
    static constexpr uint32_t HEIGHT = 720;
};
