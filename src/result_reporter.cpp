#include "result_reporter.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <cmath>

void ResultReporter::setGPUInfo(const GPUInfo& info) {
    gpuInfo_ = info;
}

void ResultReporter::setConfig(const BenchmarkConfig& config) {
    config_ = config;
}

void ResultReporter::addResults(const std::vector<BenchmarkResult>& results) {
    results_.insert(results_.end(), results.begin(), results.end());
}

void ResultReporter::setScore(const ScoreBreakdown& score) {
    score_ = score;
    hasScore_ = true;
}

std::string ResultReporter::repeatStr(const std::string& s, int n) {
    std::string result;
    for (int i = 0; i < n; i++) result += s;
    return result;
}

void ResultReporter::printReport() const {
    const std::string BOLD = "\033[1m";
    const std::string CYAN = "\033[36m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string MAGENTA = "\033[35m";
    const std::string RESET = "\033[0m";
    const std::string DIM = "\033[2m";

    std::string line = repeatStr("═", 60);
    std::string thinLine = repeatStr("─", 60);

    std::cout << "\n";
    std::cout << BOLD << CYAN << "╔" << line << "╗" << RESET << "\n";
    std::cout << BOLD << CYAN << "║" << RESET
              << BOLD << "  CLIBench v" << CLIBENCH_VERSION
              << "  -  GPU Benchmark"
              << std::string(60 - 36, ' ') << CYAN << "║" << RESET << "\n";
    std::cout << BOLD << CYAN << "╚" << line << "╝" << RESET << "\n";

    // Mode info
    std::cout << "\n" << BOLD << " Configuration" << RESET << "\n";
    std::cout << " " << thinLine << "\n";
    std::cout << "  Mode:       " << BOLD << BenchmarkConfig::modeToString(config_.mode) << RESET << "\n";
    std::cout << "  Log Level:  " << BenchmarkConfig::logLevelToString(config_.logLevel) << "\n";

    // GPU Info
    std::cout << "\n" << BOLD << " GPU Information" << RESET << "\n";
    std::cout << " " << thinLine << "\n";
    std::cout << "  Device:     " << BOLD << gpuInfo_.name << RESET << "\n";
    std::cout << "  Type:       " << gpuInfo_.deviceType << "\n";
    std::cout << "  API:        " << gpuInfo_.backend;
    if (!gpuInfo_.apiVersion.empty() && gpuInfo_.apiVersion != "N/A")
        std::cout << " " << gpuInfo_.apiVersion;
    std::cout << "\n";
    std::cout << "  Driver:     " << gpuInfo_.driverVersion << "\n";
    std::cout << "  VRAM:       " << gpuInfo_.deviceLocalMemoryMB << " MB\n";
    std::cout << "  Max WG:     " << gpuInfo_.maxComputeWorkGroupInvocations << " invocations\n";

    // Results
    if (!results_.empty()) {
        std::cout << "\n" << BOLD << " Benchmark Results" << RESET << "\n";
        std::cout << " " << thinLine << "\n";

        size_t maxNameLen = 0;
        for (const auto& r : results_) {
            maxNameLen = std::max(maxNameLen, r.name.length());
        }
        maxNameLen = std::max(maxNameLen, static_cast<size_t>(22));

        std::cout << "  " << BOLD << std::left << std::setw(maxNameLen + 2) << "Test"
                  << std::right << std::setw(14) << "Result"
                  << std::setw(12) << "Unit" << RESET << "\n";
        std::cout << "  " << repeatStr("─", maxNameLen + 28) << "\n";

        for (const auto& r : results_) {
            std::cout << "  " << std::left << std::setw(maxNameLen + 2) << r.name
                      << GREEN << std::right << std::setw(14) << std::fixed << std::setprecision(2) << r.value
                      << RESET << std::setw(12) << r.unit << "\n";
        }
    }

    // Score
    if (hasScore_) {
        std::cout << "\n" << BOLD << " Overall Score" << RESET << "\n";
        std::cout << " " << thinLine << "\n";

        std::string tierColor = ScoreCalculator::getTierColor(score_.totalScore);

        if (score_.cpuScore > 0) {
            std::cout << "  CPU:        " << YELLOW << std::right << std::setw(8) << std::fixed
                      << std::setprecision(0) << score_.cpuScore << RESET << " pts"
                      << DIM << "  (20%)" << RESET << "\n";
        }
        if (score_.gpuScore > 0) {
            std::cout << "  GPU:        " << YELLOW << std::right << std::setw(8) << std::fixed
                      << std::setprecision(0) << score_.gpuScore << RESET << " pts"
                      << DIM << "  (45%)" << RESET << "\n";
            if (score_.gpuComputeScore > 0 || score_.gpuGraphicsScore > 0) {
                if (score_.gpuComputeScore > 0)
                    std::cout << DIM << "    Compute:  " << std::setw(8) << std::setprecision(0)
                              << score_.gpuComputeScore << " pts" << RESET << "\n";
                if (score_.gpuGraphicsScore > 0)
                    std::cout << DIM << "    Graphics: " << std::setw(8) << std::setprecision(0)
                              << score_.gpuGraphicsScore << " pts" << RESET << "\n";
            }
        }
        if (score_.memoryScore > 0) {
            std::cout << "  Memory:     " << YELLOW << std::right << std::setw(8) << std::fixed
                      << std::setprecision(0) << score_.memoryScore << RESET << " pts"
                      << DIM << "  (35%)" << RESET << "\n";
            if (score_.memVramScore > 0 || score_.memTransferScore > 0) {
                if (score_.memVramScore > 0)
                    std::cout << DIM << "    VRAM:     " << std::setw(8) << std::setprecision(0)
                              << score_.memVramScore << " pts" << RESET << "\n";
                if (score_.memTransferScore > 0)
                    std::cout << DIM << "    Transfer: " << std::setw(8) << std::setprecision(0)
                              << score_.memTransferScore << " pts" << RESET << "\n";
            }
        }

        std::cout << "  " << repeatStr("─", 30) << "\n";
        std::cout << "  " << BOLD << "TOTAL:    " << tierColor << std::right << std::setw(8)
                  << std::fixed << std::setprecision(0) << score_.totalScore << RESET
                  << BOLD << " pts" << RESET << "\n";
        std::cout << "  " << BOLD << "Tier:     " << tierColor << score_.tier << RESET << "\n";

        // Score bar visualization
        double maxBar = 20000.0;
        int barLen = static_cast<int>(std::max(0.0, score_.totalScore / maxBar) * 40.0);
        barLen = std::clamp(barLen, 0, 40);
        std::cout << "\n  ";
        for (int i = 0; i < barLen; i++) {
            if (i < 10)      std::cout << "\033[36m█";
            else if (i < 20) std::cout << "\033[32m█";
            else if (i < 30) std::cout << "\033[33m█";
            else             std::cout << "\033[35m█";
        }
        for (int i = barLen; i < 40; i++) std::cout << "\033[2m░\033[0m";
        std::cout << RESET << " " << std::fixed << std::setprecision(0) << score_.totalScore << "\n";
    }

    std::cout << "\n" << DIM << "  * Higher values are better" << RESET << "\n\n";
}

void ResultReporter::exportJSON(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << " for writing\n";
        return;
    }

    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", tm);

    auto escapeJSON = [](const std::string& s) -> std::string {
        std::string result;
        for (char c : s) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else result += c;
        }
        return result;
    };

    file << "{\n";
    file << "  \"tool\": \"CLIBench\",\n";
    file << "  \"version\": \"" << CLIBENCH_VERSION << "\",\n";
    file << "  \"timestamp\": \"" << timeBuf << "\",\n";
    file << "  \"mode\": \"" << BenchmarkConfig::modeToString(config_.mode) << "\",\n";
    file << "  \"gpu\": {\n";
    file << "    \"name\": \"" << escapeJSON(gpuInfo_.name) << "\",\n";
    file << "    \"type\": \"" << escapeJSON(gpuInfo_.deviceType) << "\",\n";
    file << "    \"api_version\": \"" << gpuInfo_.apiVersion << "\",\n";
    file << "    \"driver_version\": \"" << gpuInfo_.driverVersion << "\",\n";
    file << "    \"vram_mb\": " << gpuInfo_.deviceLocalMemoryMB << "\n";
    file << "  },\n";

    if (hasScore_) {
        file << "  \"score\": {\n";
        file << "    \"cpu\": " << std::fixed << std::setprecision(2) << score_.cpuScore << ",\n";
        file << "    \"gpu\": " << score_.gpuScore << ",\n";
        file << "    \"gpu_compute\": " << score_.gpuComputeScore << ",\n";
        file << "    \"gpu_graphics\": " << score_.gpuGraphicsScore << ",\n";
        file << "    \"memory\": " << score_.memoryScore << ",\n";
        file << "    \"memory_vram\": " << score_.memVramScore << ",\n";
        file << "    \"memory_transfer\": " << score_.memTransferScore << ",\n";
        file << "    \"total\": " << score_.totalScore << ",\n";
        file << "    \"tier\": \"" << escapeJSON(score_.tier) << "\"\n";
        file << "  },\n";
    }

    file << "  \"results\": [\n";
    for (size_t i = 0; i < results_.size(); i++) {
        const auto& r = results_[i];
        file << "    {\n";
        file << "      \"name\": \"" << escapeJSON(r.name) << "\",\n";
        file << "      \"value\": " << std::fixed << std::setprecision(4) << r.value << ",\n";
        file << "      \"unit\": \"" << escapeJSON(r.unit) << "\",\n";
        file << "      \"description\": \"" << escapeJSON(r.description) << "\"\n";
        file << "    }";
        if (i + 1 < results_.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    file.close();
    std::cout << "Results exported to " << filename << "\n";
}
