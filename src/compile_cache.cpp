#include "compile_cache.h"
#include <sys/stat.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>

CompileCache::CompileCache(const std::string& cacheDir, size_t maxEntries)
    : cacheDir_(cacheDir), maxEntries_(maxEntries) {
    mkdir(cacheDir_.c_str(), 0755);
}

std::string CompileCache::computeKey(const std::string& language,
                                     const std::string& code,
                                     const std::string& compileCmd) const {
    // FNV-1a 64-bit hash
    std::string combined = language + "|" + compileCmd + "|" + code;
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (char c : combined) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 0x100000001b3ULL;
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << hash;
    return oss.str();
}

bool CompileCache::get(const std::string& language, const std::string& code,
                       const std::string& compileCmd, const std::string& targetDir) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 解释型语言不做缓存
    if (compileCmd.empty()) return false;

    std::string key = computeKey(language, code, compileCmd);
    auto it = index_.find(key);
    if (it == index_.end()) return false;

    const std::string& cachePath = it->second.path;
    struct stat st;
    if (stat(cachePath.c_str(), &st) != 0) {
        // 缓存目录已被外部删除
        index_.erase(it);
        return false;
    }

    // 复制缓存产物到目标目录
    std::string cmd = "cp -r " + cachePath + "/* " + targetDir + "/ 2>/dev/null";
    if (system(cmd.c_str()) != 0) return false;

    it->second.lastAccess = time(nullptr);
    return true;
}

void CompileCache::put(const std::string& language, const std::string& code,
                       const std::string& compileCmd, const std::string& sourceDir) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 解释型语言不做缓存
    if (compileCmd.empty()) return;

    std::string key = computeKey(language, code, compileCmd);

    // 已有缓存则更新访问时间
    if (index_.count(key)) {
        index_[key].lastAccess = time(nullptr);
        return;
    }

    std::string cachePath = cacheDir_ + "/" + key;
    mkdir(cachePath.c_str(), 0755);

    std::string cmd = "cp -r " + sourceDir + "/* " + cachePath + "/ 2>/dev/null";
    if (system(cmd.c_str()) != 0) return;

    index_[key] = {cachePath, time(nullptr)};

    evictIfNeeded();
}

void CompileCache::evictIfNeeded() {
    if (index_.size() <= maxEntries_) return;

    // 按访问时间排序，找出最旧的条目
    std::vector<std::pair<std::string, time_t>> entries;
    entries.reserve(index_.size());
    for (const auto& [key, entry] : index_) {
        entries.emplace_back(key, entry.lastAccess);
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // 删除最旧的条目，直到数量低于 maxEntries_ 的 80%
    size_t target = static_cast<size_t>(maxEntries_ * 0.8);
    for (size_t i = 0; i < entries.size() && index_.size() > target; i++) {
        const std::string& key = entries[i].first;
        std::string rmCmd = "rm -rf " + index_[key].path;
        system(rmCmd.c_str());
        index_.erase(key);
    }
}

void CompileCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [key, entry] : index_) {
        std::string cmd = "rm -rf " + entry.path;
        system(cmd.c_str());
    }
    index_.clear();
}
