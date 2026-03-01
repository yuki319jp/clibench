#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"
#include "logger.h"
#include "progress.h"

#include <vector>
#include <atomic>
#include <chrono>
#include <functional>

struct StressStats {
    double avgGflops = 0.0;
    double minGflops = 1e18;
    double maxGflops = 0.0;
    double avgBandwidth = 0.0;
    double minBandwidth = 1e18;
    double maxBandwidth = 0.0;
    int totalIterations = 0;
    double totalDurationSec = 0.0;
    double stabilityPercent = 100.0; // lower variance = higher stability
    bool passed = true;
};

class StressBenchmark : public BenchmarkBase {
public:
    using RunFn = std::function<std::vector<BenchmarkResult>()>;

    StressBenchmark(const BenchmarkConfig& config, RunFn computeFn, RunFn memoryFn);
    ~StressBenchmark();

    std::string getName() const override { return "Stress Test"; }
    std::vector<BenchmarkResult> run() override;

    const StressStats& getStats() const { return stats_; }

private:
    BenchmarkConfig config_;
    RunFn computeFn_;
    RunFn memoryFn_;
    StressStats stats_;
};
