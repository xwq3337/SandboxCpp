#pragma once

#include <string>

class CgroupManager {
public:
    CgroupManager(const std::string& groupName);
    ~CgroupManager();
    
    // 设置内存限制
    int setMemoryLimit(size_t bytes);
    
    // 设置 CPU 时间限制
    int setCpuTimeLimit(int milliseconds);
    
    // 设置进程数限制
    int setPidsLimit(int max_pids);
    
    // 将进程添加到 cgroup
    int addProcess(pid_t pid);
    
    // 获取内存使用情况
    size_t getMemoryUsage();
    
    // 获取 CPU 使用时间
    long getCpuUsage();
    
    // 清理 cgroup
    void cleanup();

private:
    std::string groupName_;
    std::string cgroupPath_;
    
    std::string getCgroupPath(const std::string& controller);
    int writeToFile(const std::string& path, const std::string& value);
    std::string readFromFile(const std::string& path);
};
