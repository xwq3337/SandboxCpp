#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>

class CompileCache {
public:
    CompileCache(const std::string& cacheDir, size_t maxEntries = 500);

    // 查找缓存，命中则复制到 targetDir 并返回 true
    bool get(const std::string& language, const std::string& code,
             const std::string& compileCmd, const std::string& targetDir);

    // 编译成功后存入缓存
    void put(const std::string& language, const std::string& code,
             const std::string& compileCmd, const std::string& sourceDir);

    // 清空缓存
    void clear();

private:
    std::string computeKey(const std::string& language, const std::string& code,
                           const std::string& compileCmd) const;
    void evictIfNeeded();

    std::mutex mutex_;
    std::string cacheDir_;
    size_t maxEntries_;

    struct Entry {
        std::string path;
        time_t lastAccess;
    };
    std::unordered_map<std::string, Entry> index_;
};
