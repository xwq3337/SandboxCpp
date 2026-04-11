#include "stats_collector.h"

StatsCollector::StatsCollector()
    : start_time_(std::chrono::steady_clock::now()) {}

void StatsCollector::record(const OutputResult& output) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 计算所有测试用例的总耗时
    int total_case_time = 0;
    for (const auto& tc : output.result) {
        total_case_time += tc.time;
    }

    // 更新全局统计（在线均值）
    total_submissions_++;
    global_avg_max_time_ms_ +=
        (output.max_time - global_avg_max_time_ms_) / total_submissions_;
    global_avg_total_time_ms_ +=
        (total_case_time - global_avg_total_time_ms_) / total_submissions_;
    global_total_test_cases_ += output.result.size();

    // 更新按语言统计
    auto& lang = per_language_[output.language];
    lang.count++;
    lang.avg_max_time_ms +=
        (output.max_time - lang.avg_max_time_ms) / lang.count;
    lang.avg_total_time_ms +=
        (total_case_time - lang.avg_total_time_ms) / lang.count;
    lang.total_test_cases += output.result.size();

    // 更新判定结果计数
    verdict_counts_[output.verdict]++;
}

json StatsCollector::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);

    double uptime_sec = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start_time_).count();

    json stats;
    stats["uptime_seconds"] = uptime_sec;
    stats["total_submissions"] = total_submissions_;
    stats["total_test_cases"] = global_total_test_cases_;
    stats["throughput_submissions_per_sec"] =
        uptime_sec > 0 ? total_submissions_ / uptime_sec : 0.0;

    stats["global"] = {
        {"avg_max_time_ms", global_avg_max_time_ms_},
        {"avg_total_test_case_time_ms", global_avg_total_time_ms_}
    };

    // 按语言统计
    json per_lang = json::object();
    for (const auto& [name, ls] : per_language_) {
        per_lang[name] = {
            {"count", ls.count},
            {"avg_max_time_ms", ls.avg_max_time_ms},
            {"avg_total_test_case_time_ms", ls.avg_total_time_ms},
            {"total_test_cases", ls.total_test_cases}
        };
    }
    stats["per_language"] = per_lang;

    // 判定结果分布
    json verdicts = json::object();
    for (const auto& [v, count] : verdict_counts_) {
        verdicts[json(v).get<std::string>()] = count;
    }
    stats["verdict_counts"] = verdicts;

    return stats;
}

void StatsCollector::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    total_submissions_ = 0;
    global_avg_max_time_ms_ = 0.0;
    global_avg_total_time_ms_ = 0.0;
    global_total_test_cases_ = 0;
    start_time_ = std::chrono::steady_clock::now();
    per_language_.clear();
    verdict_counts_.clear();
}
