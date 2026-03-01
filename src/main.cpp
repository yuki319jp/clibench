#include "benchmark_config.h"
#include "vulkan_context.h"
#include "cpu_benchmark.h"
#include "compute_benchmark.h"
#include "memory_benchmark.h"
#include "graphics_benchmark.h"
#include "stress_benchmark.h"
#include "result_reporter.h"
#include "score_calculator.h"
#include "logger.h"

#ifdef CLIBENCH_HAS_METAL
#include "metal_context.h"
#include "metal_compute_benchmark.h"
#include "metal_memory_benchmark.h"
#include "metal_graphics_benchmark.h"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <cstring>

static void printUsage() {
    std::cout << R"(
CLIBench v)" << CLIBENCH_VERSION << R"( - Cross-Platform GPU Benchmark

Usage: clibench [options]

Backend:
  --vulkan              Use Vulkan backend (default on Linux/Windows)
  --metal               Use Metal backend (macOS only, default on macOS)

Modes:
  --quick               Quick mode (reduced iterations, fast results)
  --standard            Standard mode (default, balanced)
  --extreme             Extreme mode (max iterations, thorough)
  --stress [duration]   Stress test mode (default: 60s)

Benchmark Selection:
  --cpu-only            Run only CPU benchmark
  --compute-only        Run only compute benchmark
  --memory-only         Run only memory benchmark
  --graphics-only       Run only graphics benchmark
  --no-cpu              Skip CPU benchmark
  --no-compute          Skip compute benchmark
  --no-memory           Skip memory benchmark
  --no-graphics         Skip graphics benchmark

GPU:
  --list-gpus           List available GPUs
  --gpu <index>         Select GPU by index

Custom Parameters:
  --cpu-threads <n>         Number of CPU threads (default: auto)
  --cpu-iters <n>           CPU iterations per thread
  --compute-elements <n>    Number of compute elements (vec4)
  --compute-iters <n>       Compute shader iterations
  --compute-runs <n>        Number of compute runs
  --memory-size <mb>        Memory buffer size in MB
  --memory-iters <n>        Memory transfer iterations
  --graphics-frames <n>     Graphics benchmark frame count
  --graphics-drawcalls <n>  Draw calls per frame
  --stress-load <0.0-1.0>   Stress test GPU load factor

Output:
  --json <file>         Export results to JSON
  --no-score            Disable overall score calculation
  --no-progress         Disable progress bars

Verbosity:
  --quiet, -q           Only show final results
  --verbose, -v         Show detailed timing information
  --debug               Show all debug information

Score System:
  CPU (20%), GPU (45%), Memory (35%) - all affect overall score.
  GPU = Compute (25%) + Graphics (20%)
  Memory = VRAM Bandwidth (20%) + Transfer (15%)

General:
  --help, -h            Show this help message

Examples:
  clibench                           Standard benchmark (auto backend)
  clibench --metal                   Use Metal backend (macOS)
  clibench --vulkan --quick          Vulkan quick benchmark
  clibench --extreme --verbose       Thorough with details
  clibench --stress 120              2-minute stress test
  clibench --compute-iters 1024      Custom compute iterations
  clibench --json out.json -q        Quiet with JSON export
)";
}

static BenchmarkConfig parseArgs(int argc, char* argv[]) {
    BenchmarkConfig cfg;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        // Backend selection
        if (arg == "--vulkan")       cfg.backend = BackendType::Vulkan;
        else if (arg == "--metal")   cfg.backend = BackendType::Metal;
        // Modes
        else if (arg == "--quick")         cfg.mode = BenchmarkMode::Quick;
        else if (arg == "--standard") cfg.mode = BenchmarkMode::Standard;
        else if (arg == "--extreme")  cfg.mode = BenchmarkMode::Extreme;
        else if (arg == "--stress") {
            cfg.mode = BenchmarkMode::Stress;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                cfg.stressDurationSec = std::stoi(argv[++i]);
            }
        }
        // Benchmark selection
        else if (arg == "--cpu-only") {
            cfg.runCPU = true; cfg.runCompute = false; cfg.runMemory = false; cfg.runGraphics = false;
        }
        else if (arg == "--compute-only") {
            cfg.runCPU = false; cfg.runCompute = true; cfg.runMemory = false; cfg.runGraphics = false;
        }
        else if (arg == "--memory-only") {
            cfg.runCPU = false; cfg.runCompute = false; cfg.runMemory = true; cfg.runGraphics = false;
        }
        else if (arg == "--graphics-only") {
            cfg.runCPU = false; cfg.runCompute = false; cfg.runMemory = false; cfg.runGraphics = true;
        }
        else if (arg == "--no-cpu")      cfg.runCPU = false;
        else if (arg == "--no-compute")  cfg.runCompute = false;
        else if (arg == "--no-memory")   cfg.runMemory = false;
        else if (arg == "--no-graphics") cfg.runGraphics = false;
        // GPU
        else if (arg == "--list-gpus") cfg.listGPUs = true;
        else if (arg == "--gpu" && i + 1 < argc) cfg.gpuIndex = std::stoi(argv[++i]);
        // Custom params
        else if (arg == "--cpu-threads" && i + 1 < argc)  cfg.cpuThreads = std::stoul(argv[++i]);
        else if (arg == "--cpu-iters" && i + 1 < argc)    cfg.cpuIterations = std::stoull(argv[++i]);
        else if (arg == "--compute-elements" && i + 1 < argc) cfg.computeElements = std::stoul(argv[++i]);
        else if (arg == "--compute-iters" && i + 1 < argc)    cfg.computeIterations = std::stoul(argv[++i]);
        else if (arg == "--compute-runs" && i + 1 < argc)     cfg.computeRuns = std::stoul(argv[++i]);
        else if (arg == "--memory-size" && i + 1 < argc)      cfg.memoryBufferSizeMB = std::stoul(argv[++i]);
        else if (arg == "--memory-iters" && i + 1 < argc)     cfg.memoryIterations = std::stoul(argv[++i]);
        else if (arg == "--graphics-frames" && i + 1 < argc)  cfg.graphicsBenchFrames = std::stoul(argv[++i]);
        else if (arg == "--graphics-drawcalls" && i + 1 < argc) cfg.graphicsDrawCalls = std::stoul(argv[++i]);
        else if (arg == "--stress-load" && i + 1 < argc)      cfg.stressTargetLoad = std::stod(argv[++i]);
        // Output
        else if (arg == "--json" && i + 1 < argc) cfg.jsonOutput = argv[++i];
        else if (arg == "--no-score")    cfg.showScore = false;
        else if (arg == "--no-progress") cfg.showProgress = false;
        // Verbosity
        else if (arg == "--quiet" || arg == "-q")   cfg.logLevel = LogLevel::Quiet;
        else if (arg == "--verbose" || arg == "-v") cfg.logLevel = LogLevel::Verbose;
        else if (arg == "--debug")                  cfg.logLevel = LogLevel::Debug;
        // Help
        else if (arg == "--help" || arg == "-h") cfg.showHelp = true;
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            cfg.showHelp = true;
        }
    }

    return cfg;
}

static void resolveBackend(BenchmarkConfig& cfg) {
    if (cfg.backend == BackendType::Auto) {
#ifdef CLIBENCH_HAS_METAL
        cfg.backend = BackendType::Metal;
        LOG_INFO("Auto-selected Metal backend (macOS native)");
#else
        cfg.backend = BackendType::Vulkan;
        LOG_INFO("Auto-selected Vulkan backend");
#endif
    }

    if (cfg.backend == BackendType::Metal) {
#ifndef CLIBENCH_HAS_METAL
        std::cerr << "Error: Metal backend is only available on macOS\n";
        exit(1);
#endif
    }
}

#ifdef CLIBENCH_HAS_METAL
static int runMetalBenchmarks(BenchmarkConfig& cfg) {
    MetalContext mtlCtx;
    mtlCtx.init(cfg.gpuIndex);

    if (cfg.listGPUs) {
        auto gpus = mtlCtx.listGPUs();
        std::cout << "Available GPUs (Metal):\n";
        for (size_t i = 0; i < gpus.size(); i++) {
            std::cout << "  [" << i << "] " << gpus[i] << "\n";
        }
        return 0;
    }

    GPUInfo gpuInfo = mtlCtx.getGPUInfo();
    LOG_INFO("GPU: " + gpuInfo.name + " (Metal)");
    LOG_INFO("Mode: " + BenchmarkConfig::modeToString(cfg.mode));

    ResultReporter reporter;
    reporter.setGPUInfo(gpuInfo);
    reporter.setConfig(cfg);

    std::vector<BenchmarkResult> allResults;

    if (cfg.mode == BenchmarkMode::Stress) {
        LOG_INFO("Starting stress test (Metal)...");

        StressBenchmark::RunFn computeFn = nullptr;
        StressBenchmark::RunFn memoryFn = nullptr;

        if (cfg.runCompute) {
            computeFn = [&]() {
                BenchmarkConfig c = cfg;
                c.computeRuns = 1; c.showProgress = false; c.logLevel = LogLevel::Quiet;
                return MetalComputeBenchmark(mtlCtx, c).run();
            };
        }
        if (cfg.runMemory) {
            memoryFn = [&]() {
                BenchmarkConfig c = cfg;
                c.memoryIterations = 1; c.showProgress = false; c.logLevel = LogLevel::Quiet;
                return MetalMemoryBenchmark(mtlCtx, c).run();
            };
        }

        StressBenchmark stress(cfg, computeFn, memoryFn);
        auto results = stress.run();
        allResults.insert(allResults.end(), results.begin(), results.end());
        reporter.addResults(results);
    } else {
        if (cfg.runCPU) {
            LOG_INFO("Running CPU benchmark...");
            CpuBenchmark bench(cfg);
            auto results = bench.run();
            allResults.insert(allResults.end(), results.begin(), results.end());
            reporter.addResults(results);
        }

        if (cfg.runCompute) {
            LOG_INFO("Running compute benchmark (Metal)...");
            MetalComputeBenchmark bench(mtlCtx, cfg);
            auto results = bench.run();
            allResults.insert(allResults.end(), results.begin(), results.end());
            reporter.addResults(results);
        }

        if (cfg.runMemory) {
            LOG_INFO("Running memory benchmark (Metal)...");
            MetalMemoryBenchmark bench(mtlCtx, cfg);
            auto results = bench.run();
            allResults.insert(allResults.end(), results.begin(), results.end());
            reporter.addResults(results);
        }

        if (cfg.runGraphics) {
            LOG_INFO("Running graphics benchmark (Metal)...");
            MetalGraphicsBenchmark bench(mtlCtx, cfg);
            auto results = bench.run();
            allResults.insert(allResults.end(), results.begin(), results.end());
            reporter.addResults(results);
        }
    }

    if (cfg.showScore && cfg.mode != BenchmarkMode::Stress) {
        auto score = ScoreCalculator::calculate(allResults);
        reporter.setScore(score);
    }

    reporter.printReport();

    if (!cfg.jsonOutput.empty()) {
        reporter.exportJSON(cfg.jsonOutput);
    }

    return 0;
}
#endif

static int runVulkanBenchmarks(BenchmarkConfig& cfg) {
    // GLFW is needed for Vulkan graphics benchmark
    bool glfwInited = glfwInit();
    if (!glfwInited) {
        LOG_WARN("Failed to initialize GLFW. Graphics benchmark will be skipped.");
        cfg.runGraphics = false;
    }

    try {
        VulkanContext ctx;
        ctx.init(cfg.gpuIndex);

        if (cfg.listGPUs) {
            auto gpus = ctx.listGPUs();
            std::cout << "Available GPUs (Vulkan):\n";
            for (size_t i = 0; i < gpus.size(); i++) {
                std::cout << "  [" << i << "] " << gpus[i] << "\n";
            }
            ctx.cleanup();
            if (glfwInited) glfwTerminate();
            return 0;
        }

        LOG_INFO("GPU: " + std::string(ctx.getDeviceProperties().deviceName) + " (Vulkan)");
        LOG_INFO("Mode: " + BenchmarkConfig::modeToString(cfg.mode));

        ResultReporter reporter;
        reporter.setGPUInfo(ctx.getGPUInfo());
        reporter.setConfig(cfg);

        std::vector<BenchmarkResult> allResults;

        if (cfg.mode == BenchmarkMode::Stress) {
            LOG_INFO("Starting stress test (Vulkan)...");

            StressBenchmark::RunFn computeFn = nullptr;
            StressBenchmark::RunFn memoryFn = nullptr;

            if (cfg.runCompute && ctx.getQueueFamilyIndices().hasCompute()) {
                computeFn = [&]() {
                    BenchmarkConfig c = cfg;
                    c.computeRuns = 1; c.showProgress = false; c.logLevel = LogLevel::Quiet;
                    return ComputeBenchmark(ctx, c).run();
                };
            }
            if (cfg.runMemory) {
                memoryFn = [&]() {
                    BenchmarkConfig c = cfg;
                    c.memoryIterations = 1; c.showProgress = false; c.logLevel = LogLevel::Quiet;
                    return MemoryBenchmark(ctx, c).run();
                };
            }

            StressBenchmark stress(cfg, computeFn, memoryFn);
            auto results = stress.run();
            allResults.insert(allResults.end(), results.begin(), results.end());
            reporter.addResults(results);
        } else {
            if (cfg.runCPU) {
                LOG_INFO("Running CPU benchmark...");
                CpuBenchmark bench(cfg);
                auto results = bench.run();
                allResults.insert(allResults.end(), results.begin(), results.end());
                reporter.addResults(results);
            }

            if (cfg.runCompute) {
                LOG_INFO("Running compute benchmark (Vulkan)...");
                ComputeBenchmark bench(ctx, cfg);
                auto results = bench.run();
                allResults.insert(allResults.end(), results.begin(), results.end());
                reporter.addResults(results);
            }

            if (cfg.runMemory) {
                LOG_INFO("Running memory benchmark (Vulkan)...");
                MemoryBenchmark bench(ctx, cfg);
                auto results = bench.run();
                allResults.insert(allResults.end(), results.begin(), results.end());
                reporter.addResults(results);
            }

            if (cfg.runGraphics) {
                LOG_INFO("Running graphics benchmark (Vulkan)...");
                GraphicsBenchmark bench(ctx, cfg);
                auto results = bench.run();
                allResults.insert(allResults.end(), results.begin(), results.end());
                reporter.addResults(results);
            }
        }

        if (cfg.showScore && cfg.mode != BenchmarkMode::Stress) {
            auto score = ScoreCalculator::calculate(allResults);
            reporter.setScore(score);
        }

        reporter.printReport();

        if (!cfg.jsonOutput.empty()) {
            reporter.exportJSON(cfg.jsonOutput);
        }

    } catch (const std::exception& e) {
        LOG_ERROR(e.what());
        if (glfwInited) glfwTerminate();
        return 1;
    }

    if (glfwInited) glfwTerminate();
    return 0;
}

int main(int argc, char* argv[]) {
    BenchmarkConfig cfg = parseArgs(argc, argv);

    if (cfg.showHelp) {
        printUsage();
        return 0;
    }

    cfg.resolveDefaults();
    Logger::instance().setLevel(cfg.logLevel);

    LOG_DEBUG("CLIBench v" + std::string(CLIBENCH_VERSION));
    LOG_DEBUG("Mode: " + BenchmarkConfig::modeToString(cfg.mode));

    resolveBackend(cfg);
    LOG_DEBUG("Backend: " + BenchmarkConfig::backendToString(cfg.backend));

#ifdef CLIBENCH_HAS_METAL
    if (cfg.backend == BackendType::Metal) {
        return runMetalBenchmarks(cfg);
    }
#endif

    return runVulkanBenchmarks(cfg);
}
