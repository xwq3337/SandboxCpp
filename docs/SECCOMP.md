# Seccomp 系统调用拦截

## 概述

Seccomp (Secure Computing Mode) 是 Linux 内核提供的一种安全机制，用于限制进程可以执行的系统调用。本项目的 `SeccompFilter` 类基于 `libseccomp` 实现白名单模式的系统调用过滤，只允许预定义的安全系统调用通过，任何不在白名单中的系统调用将导致进程被 `SIGSYS` 信号杀死，判题系统会将此判定为 `RestrictedSystemCall`。

## 核心实现

源码文件：[src/seccomp_filter.cpp](../src/seccomp_filter.cpp) / [include/seccomp_filter.h](../include/seccomp_filter.h)

### 类接口

```cpp
class SeccompFilter {
public:
    // 安装 seccomp 过滤器
    static int installFilter(const std::vector<std::string>& allowedSyscalls);

    // 获取预定义的系统调用白名单（通用 fallback）
    static std::vector<std::string> getDefaultAllowedSyscalls();

private:
    // 系统调用名称到编号的映射（x86_64）
    static std::map<std::string, int> getSyscallMap();
};
```

### 安装流程

`SeccompFilter::installFilter()` 的执行流程：

```
1. seccomp_init(SCMP_ACT_KILL)
   └── 创建 seccomp 上下文，默认动作：杀死进程 (SIGSYS)

2. for each syscall in allowedSyscalls:
   ├── 从 syscallMap 查找名称 → 编号
   ├── 或通过 libseccomp 的 seccomp_syscall_resolve_name() 解析
   └── seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscallNum, 0)

3. seccomp_load(ctx)
   └── 将过滤器加载到内核，开始生效

4. seccomp_release(ctx)
   └── 释放上下文资源
```

关键设计决策：使用 **SCMP_ACT_KILL** 作为默认动作，意味着不在白名单中的系统调用会立即杀死进程。这遵循最小权限原则。

### 启用条件

在 `code_executor.cpp` 中，seccomp 过滤器**不是无条件启用的**：

```cpp
// 安装 seccomp 过滤器：仅当 allowedSyscalls 非空且不是默认列表时启用
static const std::vector<std::string> defaultSyscalls =
    SeccompFilter::getDefaultAllowedSyscalls();
if (!allowedSyscalls.empty() && allowedSyscalls != defaultSyscalls) {
    if (SeccompFilter::installFilter(allowedSyscalls) != 0) {
        fprintf(stderr, "Failed to install seccomp filter\n");
        exit(1);
    }
}
```

只有当调用方通过 `seccomp_profile` 字段显式传入自定义白名单时，过滤器才会被安装。默认白名单仅用于参考，不会实际安装过滤器。这保证了正常评测不受限制，同时允许特定场景下启用系统调用过滤。

### 系统调用映射

`getSyscallMap()` 维护了一个 `x86_64` 架构下常用系统调用名称到编号的映射表：

| 系统调用 | 编号 | 用途 |
|---|---|---|
| read | 0 | 读取文件 |
| write | 1 | 写入文件 |
| open | 2 | 打开文件 |
| close | 3 | 关闭文件 |
| mmap | 9 | 内存映射 |
| mprotect | 10 | 修改内存保护 |
| rt_sigaction | 13 | 信号处理 |
| exit | 60 | 进程退出 |
| futex | 202 | 快速用户空间互斥锁 |
| clock_gettime | 228 | 获取时间 |
| ... | ... | ... |

对于不在映射表中的系统调用，代码会通过 `seccomp_syscall_resolve_name()` 让 libseccomp 动态解析。

## 默认白名单

`getDefaultAllowedSyscalls()` 返回一个通用的系统调用白名单，包含以下几类：

### 基础 I/O
`read`, `write`, `close`, `readv`, `writev`, `pread64`, `pwrite64`

### 内存管理
`mmap`, `mprotect`, `munmap`, `brk`, `mremap`, `madvise`

### 文件操作
`open`, `openat`, `stat`, `lstat`, `fstat`, `readlink`, `getcwd`, `chdir`, `stat64`, `fstat64`

### 进程控制
`exit`, `exit_group`, `fork`, `vfork`, `clone`, `execve`, `getpid`

### 信号处理
`rt_sigaction`, `rt_sigprocmask`, `rt_sigreturn`, `rt_sigpending`, `rt_sigsuspend`, `sigaltstack`

### 时间相关
`clock_gettime`, `gettimeofday`, `time`, `clock_nanosleep`, `nanosleep`, `times`

### 同步原语
`futex`, `set_robust_list`

### 系统信息
`getuid`, `getgid`, `geteuid`, `getegid`, `uname`, `getrandom`

### 管道
`pipe`, `pipe2`, `dup`, `dup2`, `dup3`

### 资源管理
`getrlimit`, `setrlimit`, `getrusage`, `prlimit64`, `sched_getaffinity`

### 架构相关
`arch_prctl`, `set_tid_address`

## 语言级白名单

每种语言在 `config/languages.json` 中有独立的 `allow_sys_calls` 列表。不同语言需要不同的系统调用：

| 语言 | 额外需要的特殊系统调用 |
|---|---|
| C/C++ | 无特殊要求（白名单已覆盖基本需求） |
| Python | `poll`, `ioctl`, `fcntl`, `select`, `epoll_*`, `getdents`, `readlink`, `getcwd`, `chdir`, `time`, `pselect6` |
| Java | `clone`, `socket`, `connect`, `accept`, `bind`, `listen`, `sendto`, `recvfrom`, `setsockopt`, `getsockopt`, `sched_yield` |
| Go | `clone`, `sched_yield`, `nanosleep`, `sigaltstack`, `tgkill`, `poll` |
| JavaScript/Node.js | `poll`, `ioctl`, `fcntl`, `select`, `epoll_*`, `getdents`, `readlink`, `getcwd`, `chdir`, `time`, `pselect6`, `clone`, `nanosleep`, `socket`, `connect`, `bind`, `listen`, `accept` |
| C#/Mono | `poll`, `ioctl`, `fcntl`, `select`, `epoll_*`, `socket`, `connect`, `accept`, `bind`, `listen`, `clone`, `sched_yield` |

## 判定逻辑

当 seccomp 过滤器安装后，进程如果调用不在白名单中的系统调用，内核会发送 `SIGSYS` 信号终止进程。判题系统在等待子进程后检测此信号：

```cpp
} else if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
    // ...
    } else if (sig == SIGSYS) {
        result.status = Verdict::RestrictedSystemCall;
    }
    // ...
}
```

## 测试示例

```cpp
// 启用 seccomp 并指定只允许基本系统调用
// 代码尝试调用 socket() → 不在白名单中 → SIGSYS → RestrictedSystemCall
InputStruct input{
    0, "cpp",
    "#include <unistd.h>\n#include <sys/syscall.h>\n#include <sys/socket.h>\n"
    "int main() {\n    syscall(SYS_socket, AF_INET, SOCK_STREAM, 0);\n    return 0;\n}",
    {{1, "", ""}},
    {1000, 268435456, 8388608, 1048576},
    "",
    R"(["read","write","close","fstat","mmap","mprotect","munmap","brk",)"
    R"("rt_sigaction","rt_sigprocmask","rt_sigreturn","access","exit",)"
    R"("exit_group","open","openat","stat","lstat","execve","getpid"])"
};
```

## 系统架构支持

当前 `getSyscallMap()` 中的映射表仅覆盖 `__x86_64__` 架构。在 ARM64 (`aarch64`) 等其他架构上，依赖 `libseccomp` 的 `seccomp_syscall_resolve_name()` 函数动态解析系统调用名称，因此仍然可以正常工作。

## 安全考量

1. **最小权限原则**：默认杀死，显式放行
2. **语言级定制**：不同语言有不同的系统调用需求，各自维护独立白名单
3. **按需启用**：不强制安装 seccomp，通过 `seccomp_profile` 字段控制
4. **防御深度**：seccomp 是三层沙箱（cgroup + namespace + seccomp）的最内层防线
