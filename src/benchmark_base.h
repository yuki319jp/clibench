#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <thread>

struct BenchmarkResult {
    std::string name;
    std::string unit;
    double value;
    std::string description;
};

class BenchmarkBase {
public:
    virtual ~BenchmarkBase() = default;
    virtual std::string getName() const = 0;
    virtual std::vector<BenchmarkResult> run() = 0;

protected:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    double elapsedMs(TimePoint start, TimePoint end) const {
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    double elapsedSec(TimePoint start, TimePoint end) const {
        return std::chrono::duration<double>(end - start).count();
    }
};
