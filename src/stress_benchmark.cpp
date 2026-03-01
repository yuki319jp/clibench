#include "stress_benchmark.h"
#include <cmath>
#include <numeric>

StressBenchmark::StressBenchmark(const BenchmarkConfig& config, RunFn computeFn, RunFn memoryFn)
    : config_(config), computeFn_(computeFn), memoryFn_(memoryFn) {}

StressBenchmark::~StressBenchmark() = default;

std::vector<BenchmarkResult> StressBenchmark::run() {
    bool showProgress = config_.showProgress && Logger::instance().isNormal();
    double duration = static_cast<double>(config_.stressDurationSec);

    LOG_INFO("Starting stress test (" + std::to_string(config_.stressDurationSec) + "s, load=" +
             std::to_string(static_cast<int>(config_.stressTargetLoad * 100)) + "%)");

    auto overallStart = Clock::now();

    std::vector<double> computeSamples;
    std::vector<double> memorySamples;
    int iteration = 0;

    // Create progress bar based on time
    int totalTicks = static_cast<int>(duration);
    ProgressBar progress("Stress Test", totalTicks, showProgress);
    auto lastProgressUpdate = Clock::now();

    while (true) {
        auto elapsed = elapsedSec(overallStart, Clock::now());
        if (elapsed >= duration) break;

        iteration++;

        // Run compute pass
        if (computeFn_) {
            auto results = computeFn_();
            for (auto& r : results) {
                if (r.name == "Compute (GFLOPS)") {
                    computeSamples.push_back(r.value);
                    stats_.minGflops = std::min(stats_.minGflops, r.value);
                    stats_.maxGflops = std::max(stats_.maxGflops, r.value);
                }
            }
        }

        // Run memory pass
        if (memoryFn_) {
            auto results = memoryFn_();
            for (auto& r : results) {
                if (r.name == "Host -> Device" || r.name == "Host → Device") {
                    memorySamples.push_back(r.value);
                    stats_.minBandwidth = std::min(stats_.minBandwidth, r.value);
                    stats_.maxBandwidth = std::max(stats_.maxBandwidth, r.value);
                }
            }
        }

        // Throttle based on target load
        if (config_.stressTargetLoad < 1.0) {
            auto iterEnd = Clock::now();
            double iterTime = elapsedSec(overallStart, iterEnd);
            double sleepTime = (1.0 - config_.stressTargetLoad) * 0.1; // 100ms base
            if (sleepTime > 0.0) {
                auto sleepMs = static_cast<int>(sleepTime * 1000);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }
        }

        // Update progress
        auto now = Clock::now();
        if (elapsedSec(lastProgressUpdate, now) >= 1.0) {
            int currentTick = std::min(static_cast<int>(elapsedSec(overallStart, now)), totalTicks);
            progress.update(currentTick);
            lastProgressUpdate = now;
        }
    }

    auto overallEnd = Clock::now();

    progress.finish();
    stats_.totalDurationSec = elapsedSec(overallStart, overallEnd);
    stats_.totalIterations = iteration;

    // Calculate averages
    if (!computeSamples.empty()) {
        double sum = std::accumulate(computeSamples.begin(), computeSamples.end(), 0.0);
        stats_.avgGflops = sum / computeSamples.size();

        // Stability: coefficient of variation (lower = more stable)
        double mean = stats_.avgGflops;
        double sqSum = 0.0;
        for (double v : computeSamples) sqSum += (v - mean) * (v - mean);
        double stddev = std::sqrt(sqSum / computeSamples.size());
        double cv = (mean > 0) ? (stddev / mean) * 100.0 : 0.0;
        stats_.stabilityPercent = std::max(0.0, 100.0 - cv);
    }

    if (!memorySamples.empty()) {
        double sum = std::accumulate(memorySamples.begin(), memorySamples.end(), 0.0);
        stats_.avgBandwidth = sum / memorySamples.size();
    }

    // Check if stability is acceptable (>90% = pass)
    stats_.passed = stats_.stabilityPercent >= 90.0;

    // Build results
    std::vector<BenchmarkResult> results;
    results.push_back({"Stress Duration", "sec", stats_.totalDurationSec,
        "Total stress test duration"});
    results.push_back({"Stress Iterations", "runs", static_cast<double>(stats_.totalIterations),
        "Total iteration count"});

    if (!computeSamples.empty()) {
        results.push_back({"Stress Avg GFLOPS", "GFLOPS", stats_.avgGflops, "Average compute"});
        results.push_back({"Stress Min GFLOPS", "GFLOPS", stats_.minGflops, "Minimum compute"});
        results.push_back({"Stress Max GFLOPS", "GFLOPS", stats_.maxGflops, "Maximum compute"});
    }

    if (!memorySamples.empty()) {
        results.push_back({"Stress Avg Bandwidth", "GB/s", stats_.avgBandwidth, "Average bandwidth"});
    }

    results.push_back({"Stability", "%", stats_.stabilityPercent,
        stats_.passed ? "PASSED (>90%)" : "FAILED (<90%)"});

    return results;
}
