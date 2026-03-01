#include "cpu_benchmark.h"
#include "logger.h"
#include "progress.h"
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <atomic>
#include <cmath>

// Prevent compiler from optimizing away the result
#ifdef _MSC_VER
static volatile double g_sink = 0.0;
static void doNotOptimize(double val) { g_sink = val; }
#else
static void doNotOptimize(double val) {
    asm volatile("" : : "r,m"(val) : "memory");
}
#endif

CpuBenchmark::CpuBenchmark(const BenchmarkConfig& config)
    : config_(config) {}

CpuBenchmark::~CpuBenchmark() = default;

double CpuBenchmark::cpuWorkload(uint64_t iterations) {
    // 8 independent FP64 FMA chains for ILP exploitation
    double a0 = 1.1, a1 = 1.2, a2 = 1.3, a3 = 1.4;
    double a4 = 1.5, a5 = 1.6, a6 = 1.7, a7 = 1.8;
    const double b = 0.9999999, c = 0.0000001;

    for (uint64_t i = 0; i < iterations; i++) {
        // 16 FMAs per iteration = 32 FP64 FLOPS
        a0 = a0 * b + c; a1 = a1 * b + c;
        a2 = a2 * b + c; a3 = a3 * b + c;
        a4 = a4 * b + c; a5 = a5 * b + c;
        a6 = a6 * b + c; a7 = a7 * b + c;
        a0 = a0 * b + c; a1 = a1 * b + c;
        a2 = a2 * b + c; a3 = a3 * b + c;
        a4 = a4 * b + c; a5 = a5 * b + c;
        a6 = a6 * b + c; a7 = a7 * b + c;
    }

    return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7;
}

std::vector<BenchmarkResult> CpuBenchmark::run() {
    uint32_t numThreads = config_.cpuThreads;
    if (numThreads == 0) {
        numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
    }
    uint64_t iterations = config_.cpuIterations;
    bool showProgress = config_.showProgress && Logger::instance().isNormal();

    static constexpr double FLOPS_PER_ITER = 32.0; // 16 FMAs * 2 FLOPS each

    LOG_DETAIL("CPU config: threads=" + std::to_string(numThreads) +
               " iterations=" + std::to_string(iterations));

    ProgressBar progress("CPU", 2, showProgress);

    // --- Single-threaded benchmark ---
    LOG_DEBUG("Running single-threaded CPU benchmark");
    auto stStart = Clock::now();
    double stResult = cpuWorkload(iterations);
    auto stEnd = Clock::now();
    doNotOptimize(stResult);

    double stSeconds = elapsedSec(stStart, stEnd);
    double stGflops = (static_cast<double>(iterations) * FLOPS_PER_ITER) / (stSeconds * 1e9);

    LOG_TIME("CPU single-thread", elapsedMs(stStart, stEnd));
    LOG_BENCH("CPU single-thread", stGflops, "GFLOPS");
    progress.increment();

    // --- Multi-threaded benchmark ---
    LOG_DEBUG("Running multi-threaded CPU benchmark (" + std::to_string(numThreads) + " threads)");
    std::vector<double> threadResults(numThreads, 0.0);
    std::atomic<int> threadsReady{0};

    auto mtStart = Clock::now();
    {
        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        for (uint32_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t]() {
                double result = cpuWorkload(iterations);
                doNotOptimize(result);
                threadResults[t] = result;
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }
    auto mtEnd = Clock::now();

    double mtSeconds = elapsedSec(mtStart, mtEnd);
    double mtTotalFlops = static_cast<double>(iterations) * FLOPS_PER_ITER * numThreads;
    double mtGflops = mtTotalFlops / (mtSeconds * 1e9);
    double scaling = (stGflops > 0.0) ? (mtGflops / stGflops) : 0.0;

    LOG_TIME("CPU multi-thread", elapsedMs(mtStart, mtEnd));
    LOG_BENCH("CPU multi-thread", mtGflops, "GFLOPS");
    LOG_DETAIL("Thread scaling: " + std::to_string(scaling) + "x (" +
               std::to_string(numThreads) + " threads)");
    progress.increment();

    progress.finish(std::to_string(static_cast<int>(mtGflops)) + " GFLOPS (" +
                    std::to_string(numThreads) + "T)");

    std::string desc = BenchmarkConfig::modeToString(config_.mode) + " mode, " +
                       std::to_string(numThreads) + " threads";

    return {
        {"CPU Single-Thread", "GFLOPS", stGflops, "FP64 FMA single-thread (" + desc + ")"},
        {"CPU Multi-Thread", "GFLOPS", mtGflops, "FP64 FMA multi-thread (" + desc + ")"},
    };
}
