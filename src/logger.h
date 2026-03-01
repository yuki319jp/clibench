#pragma once

#include "benchmark_config.h"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void setLevel(LogLevel level) { level_ = level; }
    LogLevel getLevel() const { return level_; }

    bool isQuiet() const   { return level_ == LogLevel::Quiet; }
    bool isNormal() const  { return level_ >= LogLevel::Normal; }
    bool isVerbose() const { return level_ >= LogLevel::Verbose; }
    bool isDebug() const   { return level_ >= LogLevel::Debug; }

    void info(const std::string& msg) {
        if (isNormal()) print("INFO", "\033[36m", msg);
    }

    void detail(const std::string& msg) {
        if (isVerbose()) print("DETAIL", "\033[34m", msg);
    }

    void debug(const std::string& msg) {
        if (isDebug()) print("DEBUG", "\033[2m", msg);
    }

    void warn(const std::string& msg) {
        if (isNormal()) print("WARN", "\033[33m", msg);
    }

    void error(const std::string& msg) {
        print("ERROR", "\033[31m", msg);
    }

    void benchmark(const std::string& name, double value, const std::string& unit) {
        if (isVerbose()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << name << ": " << value << " " << unit;
            print("BENCH", "\033[32m", oss.str());
        }
    }

    void timing(const std::string& phase, double ms) {
        if (isVerbose()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << phase << ": " << ms << " ms";
            print("TIME", "\033[35m", oss.str());
        }
    }

private:
    Logger() = default;
    LogLevel level_ = LogLevel::Normal;

    void print(const char* tag, const std::string& color, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        std::cerr << "\033[2m" << std::put_time(&tm, "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "\033[0m " << color << "[" << tag << "]\033[0m " << msg << "\n";
    }
};

// Convenience macros
#define LOG_INFO(msg)    Logger::instance().info(msg)
#define LOG_DETAIL(msg)  Logger::instance().detail(msg)
#define LOG_DEBUG(msg)   Logger::instance().debug(msg)
#define LOG_WARN(msg)    Logger::instance().warn(msg)
#define LOG_ERROR(msg)   Logger::instance().error(msg)
#define LOG_BENCH(name, val, unit) Logger::instance().benchmark(name, val, unit)
#define LOG_TIME(phase, ms) Logger::instance().timing(phase, ms)
