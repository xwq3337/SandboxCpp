#include "cgroup_manager.h"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

CgroupManager::CgroupManager(const std::string& groupName) 
    : groupName_(groupName) {
    // Cgroup v2 统一路径
    cgroupPath_ = "/sys/fs/cgroup/" + groupName_;
    
    // 创建 cgroup 目录
    mkdir(cgroupPath_.c_str(), 0755);
}

CgroupManager::~CgroupManager() {
    cleanup();
}

std::string CgroupManager::getCgroupPath(const std::string& controller) {
    // Cgroup v2 使用统一的层次结构
    return cgroupPath_;
}

int CgroupManager::writeToFile(const std::string& path, const std::string& value) {
    std::ofstream file(path);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open file: %s\n", path.c_str());
        return -1;
    }
    
    file << value;
    file.close();
    
    if (file.fail()) {
        fprintf(stderr, "Failed to write to file: %s\n", path.c_str());
        return -1;
    }
    
    return 0;
}

std::string CgroupManager::readFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open file: %s\n", path.c_str());
        return "";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int CgroupManager::setMemoryLimit(size_t bytes) {
    std::string path = cgroupPath_ + "/memory.max";
    return writeToFile(path, std::to_string(bytes));
}

int CgroupManager::setCpuTimeLimit(int milliseconds) {
    // CPU 时间限制（使用 cpu.max）
    // 格式: "quota period" (微秒)
    // 例如: "100000 1000000" 表示每1秒最多使用100ms CPU
    std::string path = cgroupPath_ + "/cpu.max";
    
    // 转换为微秒
    long quota = milliseconds * 1000;
    long period = 1000000; // 1秒
    
    std::string value = std::to_string(quota) + " " + std::to_string(period);
    return writeToFile(path, value);
}

int CgroupManager::setPidsLimit(int max_pids) {
    std::string path = cgroupPath_ + "/pids.max";
    return writeToFile(path, std::to_string(max_pids));
}

int CgroupManager::addProcess(pid_t pid) {
    std::string path = cgroupPath_ + "/cgroup.procs";
    return writeToFile(path, std::to_string(pid));
}

size_t CgroupManager::getMemoryUsage() {
    std::string path = cgroupPath_ + "/memory.current";
    std::string content = readFromFile(path);
    
    if (content.empty()) {
        return 0;
    }
    
    return std::stoull(content);
}

long CgroupManager::getCpuUsage() {
    std::string path = cgroupPath_ + "/cpu.stat";
    std::string content = readFromFile(path);
    
    if (content.empty()) {
        return 0;
    }
    
    // 解析 usage_usec
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("usage_usec") == 0) {
            size_t pos = line.find(' ');
            if (pos != std::string::npos) {
                return std::stol(line.substr(pos + 1));
            }
        }
    }
    
    return 0;
}

void CgroupManager::cleanup() {
    // 删除 cgroup 目录
    rmdir(cgroupPath_.c_str());
}
