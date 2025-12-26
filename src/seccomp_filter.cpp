#include "seccomp_filter.h"
#include <seccomp.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

int SeccompFilter::installFilter(const std::vector<std::string>& allowedSyscalls) {
    // 创建 seccomp 上下文，默认禁止所有系统调用
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
    if (ctx == nullptr) {
        fprintf(stderr, "Failed to initialize seccomp context\n");
        return -1;
    }

    // 获取系统调用映射
    auto syscallMap = getSyscallMap();

    // 添加允许的系统调用
    for (const auto& syscallName : allowedSyscalls) {
        int syscallNum = -1;
        
        // 尝试从映射中获取
        if (syscallMap.find(syscallName) != syscallMap.end()) {
            syscallNum = syscallMap[syscallName];
        } else {
            // 尝试使用 libseccomp 解析
            syscallNum = seccomp_syscall_resolve_name(syscallName.c_str());
        }

        if (syscallNum == __NR_SCMP_ERROR) {
            // 忽略不存在的系统调用，不要报错
            // fprintf(stderr, "Unknown syscall: %s\n", syscallName.c_str());
            continue;
        }

        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscallNum, 0) < 0) {
            fprintf(stderr, "Failed to add rule for syscall: %s\n", syscallName.c_str());
            seccomp_release(ctx);
            return -1;
        }
    }

    // 加载过滤器
    if (seccomp_load(ctx) < 0) {
        fprintf(stderr, "Failed to load seccomp filter\n");
        seccomp_release(ctx);
        return -1;
    }

    seccomp_release(ctx);
    return 0;
}

std::vector<std::string> SeccompFilter::getDefaultAllowedSyscalls() {
    return {
        // 基础系统调用
        "read", "write", "close", "fstat", "lseek",
        "mmap", "mprotect", "munmap", "brk",
        "rt_sigaction", "rt_sigprocmask", "rt_sigreturn",
        "access", "exit", "exit_group",
        
        // 文件操作
        "open", "openat", "stat", "lstat", "readlink",
        "getcwd", "chdir", "fstat64", "stat64",
        
        // 内存管理
        "mremap", "madvise", "brk",
        
        // 进程信息
        "getpid", "getuid", "getgid", "geteuid", "getegid",
        
        // 时间相关
        "clock_gettime", "gettimeofday", "time",
        
        // 其他必要的系统调用
        "arch_prctl", "set_tid_address", "set_robust_list",
        "futex", "sched_getaffinity", "prlimit64",
        "getrandom", "uname",
        
        // 新增：执行相关
        "execve", "clone", "fork", "vfork",
        
        // 新增：I/O 相关
        "readv", "writev", "pread64", "pwrite64",
        "dup", "dup2", "dup3", "pipe", "pipe2",
        
        // 新增：信号处理
        "rt_sigpending", "rt_sigsuspend", "sigaltstack",
        
        // 新增：其他常用调用
        "getrlimit", "setrlimit", "getrusage",
        "times", "clock_nanosleep", "nanosleep"
    };
}

std::map<std::string, int> SeccompFilter::getSyscallMap() {
    std::map<std::string, int> syscallMap;
    
    // 填充常用系统调用映射（x86_64）
    #ifdef __x86_64__
    syscallMap["read"] = 0;
    syscallMap["write"] = 1;
    syscallMap["open"] = 2;
    syscallMap["close"] = 3;
    syscallMap["stat"] = 4;
    syscallMap["fstat"] = 5;
    syscallMap["lstat"] = 6;
    syscallMap["poll"] = 7;
    syscallMap["lseek"] = 8;
    syscallMap["mmap"] = 9;
    syscallMap["mprotect"] = 10;
    syscallMap["munmap"] = 11;
    syscallMap["brk"] = 12;
    syscallMap["rt_sigaction"] = 13;
    syscallMap["rt_sigprocmask"] = 14;
    syscallMap["rt_sigreturn"] = 15;
    syscallMap["access"] = 21;
    syscallMap["exit"] = 60;
    syscallMap["exit_group"] = 231;
    syscallMap["openat"] = 257;
    syscallMap["readlink"] = 89;
    syscallMap["getcwd"] = 79;
    syscallMap["chdir"] = 80;
    syscallMap["mremap"] = 25;
    syscallMap["madvise"] = 28;
    syscallMap["getpid"] = 39;
    syscallMap["getuid"] = 102;
    syscallMap["getgid"] = 104;
    syscallMap["geteuid"] = 107;
    syscallMap["getegid"] = 108;
    syscallMap["clock_gettime"] = 228;
    syscallMap["gettimeofday"] = 96;
    syscallMap["time"] = 201;
    syscallMap["arch_prctl"] = 158;
    syscallMap["set_tid_address"] = 218;
    syscallMap["set_robust_list"] = 273;
    syscallMap["futex"] = 202;
    syscallMap["sched_getaffinity"] = 204;
    syscallMap["prlimit64"] = 302;
    syscallMap["getrandom"] = 318;
    syscallMap["uname"] = 63;
    #endif
    
    return syscallMap;
}
