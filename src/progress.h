#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>

class ProgressBar {
public:
    ProgressBar(const std::string& label, int total, bool enabled = true)
        : label_(label), total_(total), enabled_(enabled),
          start_(std::chrono::steady_clock::now()) {
        if (enabled_) render();
    }

    void update(int current) {
        if (!enabled_) return;
        current_ = std::min(current, total_);
        render();
    }

    void increment() {
        if (!enabled_) return;
        current_ = std::min(current_ + 1, total_);
        render();
    }

    void finish(const std::string& suffix = "") {
        if (!enabled_) return;
        current_ = total_;
        render();
        if (!suffix.empty()) {
            std::cerr << " " << suffix;
        }
        std::cerr << "\n";
        finished_ = true;
    }

    ~ProgressBar() {
        if (enabled_ && !finished_) {
            std::cerr << "\n";
        }
    }

private:
    void render() {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_).count();

        double fraction = (total_ > 0) ? static_cast<double>(current_) / total_ : 0.0;
        int percent = static_cast<int>(fraction * 100.0);

        // ETA calculation
        std::string eta;
        if (current_ > 0 && current_ < total_) {
            double remaining = (elapsed / current_) * (total_ - current_);
            if (remaining < 60.0) {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(0) << remaining << "s";
                eta = oss.str();
            } else {
                int mins = static_cast<int>(remaining / 60.0);
                int secs = static_cast<int>(std::fmod(remaining, 60.0));
                std::ostringstream oss;
                oss << mins << "m" << std::setfill('0') << std::setw(2) << secs << "s";
                eta = oss.str();
            }
        } else if (current_ >= total_) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1) << elapsed << "s";
            eta = oss.str();
        }

        // Bar rendering
        constexpr int BAR_WIDTH = 30;
        int filled = static_cast<int>(fraction * BAR_WIDTH);
        int empty = BAR_WIDTH - filled;

        std::ostringstream bar;
        bar << "\r  \033[36m" << label_ << "\033[0m [";

        // Gradient colors: green for filled
        for (int i = 0; i < filled; i++) {
            if (i < BAR_WIDTH / 3)       bar << "\033[32m█";   // green
            else if (i < 2 * BAR_WIDTH / 3) bar << "\033[33m█"; // yellow
            else                         bar << "\033[31m█";   // red (for extreme load)
        }
        bar << "\033[0m";

        for (int i = 0; i < empty; i++) bar << "░";

        bar << "] " << std::setw(3) << percent << "%";

        if (!eta.empty()) {
            bar << " \033[2m(" << (current_ >= total_ ? "done " : "ETA ") << eta << ")\033[0m";
        }

        // Pad to clear previous content
        std::string output = bar.str();
        std::cerr << output << "   " << std::flush;
    }

    std::string label_;
    int total_;
    int current_ = 0;
    bool enabled_;
    bool finished_ = false;
    std::chrono::steady_clock::time_point start_;
};

// Spinner for indeterminate progress
class Spinner {
public:
    Spinner(const std::string& label, bool enabled = true)
        : label_(label), enabled_(enabled),
          start_(std::chrono::steady_clock::now()) {
        if (enabled_) render();
    }

    void tick(const std::string& status = "") {
        if (!enabled_) return;
        frame_++;
        status_ = status;
        render();
    }

    void finish(const std::string& msg = "done") {
        if (!enabled_) return;
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_).count();
        std::ostringstream oss;
        oss << "\r  \033[32m✓\033[0m " << label_ << " \033[2m("
            << std::fixed << std::setprecision(1) << elapsed << "s)\033[0m";
        if (!msg.empty() && msg != "done") {
            oss << " — " << msg;
        }
        std::cerr << oss.str() << "          \n";
        finished_ = true;
    }

    ~Spinner() {
        if (enabled_ && !finished_) {
            std::cerr << "\n";
        }
    }

private:
    void render() {
        static const char* frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        constexpr int NUM_FRAMES = 10;
        auto elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_).count();

        std::ostringstream oss;
        oss << "\r  \033[36m" << frames[frame_ % NUM_FRAMES] << "\033[0m "
            << label_;
        if (!status_.empty()) {
            oss << " \033[2m" << status_ << "\033[0m";
        }
        oss << " \033[2m(" << std::fixed << std::setprecision(1) << elapsed << "s)\033[0m";
        std::cerr << oss.str() << "          " << std::flush;
    }

    std::string label_;
    std::string status_;
    bool enabled_;
    bool finished_ = false;
    int frame_ = 0;
    std::chrono::steady_clock::time_point start_;
};
