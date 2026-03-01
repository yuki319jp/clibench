#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"

class CpuBenchmark : public BenchmarkBase {
public:
    CpuBenchmark(const BenchmarkConfig& config);
    ~CpuBenchmark();

    std::string getName() const override { return "CPU"; }
    std::vector<BenchmarkResult> run() override;

private:
    // Returns a dummy value to prevent compiler from optimizing away the work
    static double cpuWorkload(uint64_t iterations);

    BenchmarkConfig config_;
};
