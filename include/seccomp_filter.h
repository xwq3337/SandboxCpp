#pragma once

#include <string>
#include <vector>
#include <map>

class SeccompFilter {
public:
    // 安装 seccomp 过滤器
    static int installFilter(const std::vector<std::string>& allowedSyscalls);
    
    // 获取预定义的系统调用白名单
    static std::vector<std::string> getDefaultAllowedSyscalls();
    
private:
    // 系统调用名称到编号的映射
    static std::map<std::string, int> getSyscallMap();
};
