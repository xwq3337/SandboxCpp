#pragma once

#include "data_structures.h"
#include <chrono>
#include <map>
#include <mutex>

struct LanguageStats {
    int64_t count = 0;
    double avg_max_time_ms = 0.0;
    double avg_total_time_ms = 0.0;
    int64_t total_test_cases = 0;
};

class StatsCollector {
public:
    StatsCollector();

    void record(const OutputResult& output);
    json toJson() const;
    void reset();

private:
    mutable std::mutex mutex_;
    int64_t total_submissions_ = 0;
    double global_avg_max_time_ms_ = 0.0;
    double global_avg_total_time_ms_ = 0.0;
    int64_t global_total_test_cases_ = 0;
    std::chrono::steady_clock::time_point start_time_;
    std::map<std::string, LanguageStats> per_language_;
    std::map<Verdict, int64_t> verdict_counts_;
};
