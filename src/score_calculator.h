#pragma once

#include "benchmark_base.h"
#include <vector>
#include <string>
#include <cmath>

struct ScoreBreakdown {
    // 3 main categories
    double cpuScore = 0.0;
    double gpuScore = 0.0;
    double memoryScore = 0.0;
    double totalScore = 0.0;
    std::string tier;

    // Sub-scores for detailed breakdown
    double gpuComputeScore = 0.0;
    double gpuGraphicsScore = 0.0;
    double memVramScore = 0.0;
    double memTransferScore = 0.0;
};

class ScoreCalculator {
public:
    // Compute overall score from benchmark results
    // Categories: CPU (20%), GPU (45%), Memory (35%)
    static ScoreBreakdown calculate(const std::vector<BenchmarkResult>& results) {
        ScoreBreakdown score;

        double cpuMt = 0.0, gpuCompute = 0.0, triRate = 0.0;
        double h2d = 0.0, d2h = 0.0, vramBw = 0.0;

        for (const auto& r : results) {
            if (r.name == "CPU Multi-Thread") {
                cpuMt = r.value;
            } else if (r.name == "Compute (GFLOPS)") {
                gpuCompute = r.value;
            } else if (r.name == "Triangle Rate") {
                triRate = r.value;
            } else if (r.name == "Host -> Device" || r.name == "Host → Device") {
                h2d = r.value;
            } else if (r.name == "Device -> Host" || r.name == "Device → Host") {
                d2h = r.value;
            } else if (r.name == "VRAM Bandwidth") {
                vramBw = r.value;
            }
        }

        // CPU Score (20% of total)
        score.cpuScore = normalize(cpuMt, REF_CPU_MT) * WEIGHT_CPU * SCALE;

        // GPU Score (45% of total) = Compute (25%) + Graphics (20%)
        score.gpuComputeScore = normalize(gpuCompute, REF_GPU_COMPUTE) * WEIGHT_GPU_COMPUTE * SCALE;
        score.gpuGraphicsScore = normalize(triRate, REF_GPU_GRAPHICS) * WEIGHT_GPU_GRAPHICS * SCALE;
        score.gpuScore = score.gpuComputeScore + score.gpuGraphicsScore;

        // Memory Score (35% of total) = VRAM BW (20%) + Transfer BW (15%)
        double avgTransfer = 0.0;
        int transferCount = 0;
        if (h2d > 0.0) { avgTransfer += h2d; transferCount++; }
        if (d2h > 0.0) { avgTransfer += d2h; transferCount++; }
        if (transferCount > 0) avgTransfer /= transferCount;

        score.memVramScore = normalize(vramBw, REF_VRAM_BW) * WEIGHT_MEM_VRAM * SCALE;
        score.memTransferScore = normalize(avgTransfer, REF_TRANSFER_BW) * WEIGHT_MEM_TRANSFER * SCALE;
        score.memoryScore = score.memVramScore + score.memTransferScore;

        score.totalScore = score.cpuScore + score.gpuScore + score.memoryScore;
        score.tier = getTier(score.totalScore);

        return score;
    }

    static std::string getTier(double score) {
        if (score >= 18000) return "S+  (Legendary)";
        if (score >= 15000) return "S   (Exceptional)";
        if (score >= 12000) return "A+  (Excellent)";
        if (score >= 10000) return "A   (Great)";
        if (score >= 8000)  return "B+  (Very Good)";
        if (score >= 6000)  return "B   (Good)";
        if (score >= 4000)  return "C+  (Above Average)";
        if (score >= 2500)  return "C   (Average)";
        if (score >= 1500)  return "D   (Below Average)";
        return "E   (Entry Level)";
    }

    static std::string getTierColor(double score) {
        if (score >= 15000) return "\033[35m";  // magenta
        if (score >= 12000) return "\033[31m";  // red
        if (score >= 10000) return "\033[33m";  // yellow
        if (score >= 6000)  return "\033[32m";  // green
        if (score >= 2500)  return "\033[36m";  // cyan
        return "\033[37m";                       // white
    }

private:
    // Reference values (mid-high range desktop baseline)
    static constexpr double REF_CPU_MT = 100.0;         // ~8-core desktop FP64 GFLOPS
    static constexpr double REF_GPU_COMPUTE = 8000.0;   // ~RTX 3070 level GFLOPS
    static constexpr double REF_GPU_GRAPHICS = 8000.0;  // ~RTX 3070 level M tri/s
    static constexpr double REF_VRAM_BW = 400.0;        // ~Mid-range GDDR6 GB/s
    static constexpr double REF_TRANSFER_BW = 20.0;     // ~PCIe 4.0 x16 GB/s

    // Scoring weights (must sum to 1.0)
    // CPU: 20%, GPU: 45% (Compute 25% + Graphics 20%), Memory: 35% (VRAM 20% + Transfer 15%)
    static constexpr double WEIGHT_CPU = 0.20;
    static constexpr double WEIGHT_GPU_COMPUTE = 0.25;
    static constexpr double WEIGHT_GPU_GRAPHICS = 0.20;
    static constexpr double WEIGHT_MEM_VRAM = 0.20;
    static constexpr double WEIGHT_MEM_TRANSFER = 0.15;

    // Score multiplier
    static constexpr double SCALE = 10000.0;

    // Power-law normalization: maps performance to a smooth scale
    // ratio=1.0 -> 1.0, ratio=2.0 -> ~1.52, ratio=0.5 -> ~0.66
    static double normalize(double value, double reference) {
        if (value <= 0.0 || reference <= 0.0) return 0.0;
        double ratio = value / reference;
        return std::pow(ratio, 0.6);
    }
};
