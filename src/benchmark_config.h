#pragma once

#include <string>
#include <cstdint>

// GPU backend selection
enum class BackendType {
    Auto,     // Metal on macOS, Vulkan elsewhere
    Vulkan,   // Vulkan (all platforms via MoltenVK on macOS)
    Metal     // Metal (macOS only)
};

// Benchmark preset modes
enum class BenchmarkMode {
    Quick,      // Fast run, reduced iterations
    Standard,   // Default balanced mode
    Extreme,    // Maximum stress, high iterations
    Stress      // Continuous loop for stability testing
};

// Log verbosity levels
enum class LogLevel {
    Quiet,    // Only final results
    Normal,   // Progress + results
    Verbose,  // Detailed timing info
    Debug     // Everything including Vulkan internals
};

struct BenchmarkConfig {
    BenchmarkMode mode = BenchmarkMode::Standard;
    LogLevel logLevel = LogLevel::Normal;
    BackendType backend = BackendType::Auto;

    // GPU selection
    int gpuIndex = -1;

    // Which benchmarks to run
    bool runCPU = true;
    bool runCompute = true;
    bool runMemory = true;
    bool runGraphics = true;

    // CPU parameters
    uint32_t cpuThreads = 0;           // 0 = auto (hardware_concurrency)
    uint64_t cpuIterations = 0;        // 0 = auto from mode

    // Compute parameters
    uint32_t computeElements = 0;       // 0 = auto from mode
    uint32_t computeIterations = 0;     // 0 = auto from mode
    uint32_t computeRuns = 0;           // 0 = auto from mode

    // Memory parameters
    uint32_t memoryBufferSizeMB = 0;    // 0 = auto from mode
    uint32_t memoryIterations = 0;      // 0 = auto from mode

    // Graphics parameters
    uint32_t graphicsWarmupFrames = 0;  // 0 = auto from mode
    uint32_t graphicsBenchFrames = 0;   // 0 = auto from mode
    uint32_t graphicsDrawCalls = 0;     // 0 = auto from mode

    // Stress mode settings
    uint32_t stressDurationSec = 60;    // seconds
    double stressTargetLoad = 1.0;      // 0.0-1.0 (percentage)

    // Output
    std::string jsonOutput;
    bool showScore = true;
    bool showProgress = true;
    bool listGPUs = false;
    bool showHelp = false;

    // Resolve 0 values to mode defaults
    void resolveDefaults() {
        switch (mode) {
        case BenchmarkMode::Quick:
            if (cpuIterations == 0)       cpuIterations = 50000000ULL;
            if (computeElements == 0)     computeElements = 256 * 1024;
            if (computeIterations == 0)   computeIterations = 128;
            if (computeRuns == 0)         computeRuns = 1;
            if (memoryBufferSizeMB == 0)  memoryBufferSizeMB = 64;
            if (memoryIterations == 0)    memoryIterations = 3;
            if (graphicsWarmupFrames == 0) graphicsWarmupFrames = 10;
            if (graphicsBenchFrames == 0)  graphicsBenchFrames = 100;
            if (graphicsDrawCalls == 0)    graphicsDrawCalls = 1000;
            break;
        case BenchmarkMode::Standard:
            if (cpuIterations == 0)       cpuIterations = 200000000ULL;
            if (computeElements == 0)     computeElements = 1024 * 1024;
            if (computeIterations == 0)   computeIterations = 512;
            if (computeRuns == 0)         computeRuns = 3;
            if (memoryBufferSizeMB == 0)  memoryBufferSizeMB = 256;
            if (memoryIterations == 0)    memoryIterations = 5;
            if (graphicsWarmupFrames == 0) graphicsWarmupFrames = 30;
            if (graphicsBenchFrames == 0)  graphicsBenchFrames = 300;
            if (graphicsDrawCalls == 0)    graphicsDrawCalls = 10000;
            break;
        case BenchmarkMode::Extreme:
            if (cpuIterations == 0)       cpuIterations = 1000000000ULL;
            if (computeElements == 0)     computeElements = 4 * 1024 * 1024;
            if (computeIterations == 0)   computeIterations = 2048;
            if (computeRuns == 0)         computeRuns = 5;
            if (memoryBufferSizeMB == 0)  memoryBufferSizeMB = 512;
            if (memoryIterations == 0)    memoryIterations = 10;
            if (graphicsWarmupFrames == 0) graphicsWarmupFrames = 60;
            if (graphicsBenchFrames == 0)  graphicsBenchFrames = 600;
            if (graphicsDrawCalls == 0)    graphicsDrawCalls = 50000;
            break;
        case BenchmarkMode::Stress:
            if (cpuIterations == 0)       cpuIterations = 200000000ULL;
            if (computeElements == 0)     computeElements = 4 * 1024 * 1024;
            if (computeIterations == 0)   computeIterations = 2048;
            if (computeRuns == 0)         computeRuns = 1;
            if (memoryBufferSizeMB == 0)  memoryBufferSizeMB = 512;
            if (memoryIterations == 0)    memoryIterations = 1;
            if (graphicsWarmupFrames == 0) graphicsWarmupFrames = 10;
            if (graphicsBenchFrames == 0)  graphicsBenchFrames = 100;
            if (graphicsDrawCalls == 0)    graphicsDrawCalls = 50000;
            break;
        }
    }

    static std::string modeToString(BenchmarkMode m) {
        switch (m) {
            case BenchmarkMode::Quick:    return "Quick";
            case BenchmarkMode::Standard: return "Standard";
            case BenchmarkMode::Extreme:  return "Extreme";
            case BenchmarkMode::Stress:   return "Stress";
        }
        return "Unknown";
    }

    static std::string logLevelToString(LogLevel l) {
        switch (l) {
            case LogLevel::Quiet:   return "Quiet";
            case LogLevel::Normal:  return "Normal";
            case LogLevel::Verbose: return "Verbose";
            case LogLevel::Debug:   return "Debug";
        }
        return "Unknown";
    }

    static std::string backendToString(BackendType b) {
        switch (b) {
            case BackendType::Auto:   return "Auto";
            case BackendType::Vulkan: return "Vulkan";
            case BackendType::Metal:  return "Metal";
        }
        return "Unknown";
    }
};
