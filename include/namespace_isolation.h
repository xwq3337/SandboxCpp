#pragma once

#include <string>

class NamespaceIsolation {
public:
    // 创建新的 namespace
    static int createNamespaces(int flags);
    
    // 挂载 /proc
    static int mountProc();
    
    // 设置 hostname
    static int setHostname(const std::string& hostname);
    
    // chroot 到指定目录
    static int chrootToDir(const std::string& path);
    
    // 准备隔离环境
    static int prepareIsolatedEnv(const std::string& rootPath);
};
