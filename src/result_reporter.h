#pragma once

#include "benchmark_base.h"
#include "benchmark_config.h"
#include "score_calculator.h"
#include "gpu_info.h"
#include <vector>
#include <string>

class ResultReporter {
public:
    void setGPUInfo(const GPUInfo& info);
    void setConfig(const BenchmarkConfig& config);
    void addResults(const std::vector<BenchmarkResult>& results);
    void setScore(const ScoreBreakdown& score);
    void printReport() const;
    void exportJSON(const std::string& filename) const;

private:
    GPUInfo gpuInfo_{};
    BenchmarkConfig config_;
    std::vector<BenchmarkResult> results_;
    ScoreBreakdown score_;
    bool hasScore_ = false;

    static std::string repeatStr(const std::string& s, int n);
};
