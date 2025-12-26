#include "namespace_isolation.h"
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>

int NamespaceIsolation::createNamespaces(int flags) {
    // 使用 unshare 创建新的 namespace
    if (unshare(flags) != 0) {
        fprintf(stderr, "Failed to create namespaces: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int NamespaceIsolation::mountProc() {
    // 挂载 /proc 文件系统
    if (mount("proc", "/proc", "proc", 0, nullptr) != 0) {
        fprintf(stderr, "Failed to mount /proc: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int NamespaceIsolation::setHostname(const std::string& hostname) {
    if (sethostname(hostname.c_str(), hostname.length()) != 0) {
        fprintf(stderr, "Failed to set hostname: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int NamespaceIsolation::chrootToDir(const std::string& path) {
    // 切换到新的根目录
    if (chroot(path.c_str()) != 0) {
        fprintf(stderr, "Failed to chroot to %s: %s\n", path.c_str(), strerror(errno));
        return -1;
    }
    
    // 切换工作目录到根
    if (chdir("/") != 0) {
        fprintf(stderr, "Failed to chdir to /: %s\n", strerror(errno));
        return -1;
    }
    
    return 0;
}

int NamespaceIsolation::prepareIsolatedEnv(const std::string& rootPath) {
    // 创建必要的目录结构
    mkdir((rootPath + "/proc").c_str(), 0755);
    mkdir((rootPath + "/tmp").c_str(), 0755);
    mkdir((rootPath + "/dev").c_str(), 0755);
    
    // 创建基本设备节点
    // 注意：这需要 root 权限
    // mknod((rootPath + "/dev/null").c_str(), S_IFCHR | 0666, makedev(1, 3));
    // mknod((rootPath + "/dev/zero").c_str(), S_IFCHR | 0666, makedev(1, 5));
    
    return 0;
}
