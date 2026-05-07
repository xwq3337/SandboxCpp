# Cgroups 资源限制管理

## 概述

Cgroups (Control Groups) 是 Linux 内核提供的资源限制、优先级分配和资源监控机制。本项目使用 **cgroup v2** (统一层级结构) 实现对代码执行进程的 CPU 时间、内存使用和进程数的精确控制。

源码文件：[src/cgroup_manager.cpp](../src/cgroup_manager.cpp) / [include/cgroup_manager.h](../include/cgroup_manager.h)

## Cgroup v1 vs v2

| 特性 | Cgroup v1 | Cgroup v2 |
|---|---|---|
| 层级结构 | 每个控制器独立层级 | 统一层级 |
| 路径 | `/sys/fs/cgroup/<controller>/<group>/` | `/sys/fs/cgroup/<group>/` |
| 内存限制文件 | `memory.limit_in_bytes` | `memory.max` |
| CPU 限制文件 | `cpu.cfs_quota_us` / `cpu.cfs_period_us` | `cpu.max` |
| 进程管理文件 | `tasks` | `cgroup.procs` |
| 线程控制 | `tasks` (混合线程和进程) | `cgroup.threads` |
| 内核支持 | Linux 2.6.24+ | Linux 4.5+（推荐 5.0+） |

本项目**仅支持 cgroup v2**。可以通过以下命令确认系统使用的版本：

```bash
# 检查 cgroup v2 是否挂载
mount | grep cgroup2
# 输出示例: cgroup2 on /sys/fs/cgroup type cgroup2 (...)

# 或检查根目录下的文件
ls /sys/fs/cgroup/
# 如果看到 cgroup.controllers、memory.max 等文件，则是 cgroup v2
```

## 核心实现

### 类接口

```cpp
class CgroupManager {
public:
    explicit CgroupManager(const std::string& groupName);
    ~CgroupManager();

    // 设置内存限制（字节）
    int setMemoryLimit(size_t bytes);

    // 设置 CPU 时间限制（毫秒/秒配额模式）
    int setCpuTimeLimit(int milliseconds);

    // 设置最大进程数
    int setPidsLimit(int max_pids);

    // 将进程添加到 cgroup
    int addProcess(pid_t pid);

    // 获取当前内存使用量（字节）
    size_t getMemoryUsage();

    // 获取 CPU 使用时间（微秒）
    long getCpuUsage();

private:
    void cleanup();                              // 删除 cgroup 目录
    int writeToFile(const std::string& path, const std::string& value);
    std::string readFromFile(const std::string& path);

    std::string cgroupPath_;                     // cgroup 目录路径
    std::string groupName_;                      // cgroup 名称
};
```

### 生命周期

```
构造函数: 创建 cgroup 目录 /sys/fs/cgroup/<groupName>/
    ↓
setMemoryLimit()  → 写入 memory.max
setCpuTimeLimit() → 写入 cpu.max
setPidsLimit()    → 写入 pids.max
    ↓
addProcess()      → 写入 cgroup.procs（加入进程）
    ↓
[进程运行，受 cgroup 限制]
    ↓
析构函数: cleanup() → rmdir 删除 cgroup 目录
```

## 资源限制详解

### 1. 内存限制

```cpp
int CgroupManager::setMemoryLimit(size_t bytes) {
    std::string path = cgroupPath_ + "/memory.max";
    return writeToFile(path, std::to_string(bytes));
}
```

通过写入 `memory.max` 文件设置硬性内存上限。进程的内存使用（包括匿名页、文件缓存等）达到此限制时，cgroup 的 OOM killer 会向进程发送 `SIGKILL`。

**判题映射：**
```cpp
if (sig == SIGKILL && result.memory > (size_t)limits.memory_bytes) {
    result.status = Verdict::MemoryLimitExceeded;
}
```

**Fallback 机制：** 当 cgroup 的 `memory.current` 返回 0（如内核参数 `cgroup.memory=nokmem` 导致内存统计不可用时），代码会使用 `wait4()` 返回的 `rusage.ru_maxrss` 作为内存使用量的近似值：

```cpp
result.memory = cgroup.getMemoryUsage();
if (result.memory == 0 && usage.ru_maxrss > 0) {
    result.memory = usage.ru_maxrss * 1024; // ru_maxrss 单位是 KB
}
```

此外，当 `limits.memory_bytes < 100MB` 时，还会额外启用 `setrlimit(RLIMIT_AS)` 作为补充的内存限制手段。

### 2. CPU 时间限制

```cpp
int CgroupManager::setCpuTimeLimit(int milliseconds) {
    std::string path = cgroupPath_ + "/cpu.max";
    long quota = milliseconds * 1000;   // 转换为微秒
    long period = 1000000;              // 1 秒周期
    std::string value = std::to_string(quota) + " " + std::to_string(period);
    return writeToFile(path, value);
}
```

`cpu.max` 文件格式为 `$MAX $PERIOD`（单位：微秒）。语义为：在每个 `$PERIOD` 微秒的周期内，进程组最多可使用 `$MAX` 微秒的 CPU 时间。

例如 `"100000 1000000"` 表示每 1 秒最多使用 100ms CPU 时间（即 10% CPU）。

**注意：** 当前判题逻辑中，CPU 超限的判定同时依赖 `setrlimit(RLIMIT_CPU)` 和代码执行时间测量。`cpu.max` 的 cgroup 限制作为额外的保护层。

### 3. 进程数限制

```cpp
int CgroupManager::setPidsLimit(int max_pids) {
    std::string path = cgroupPath_ + "/pids.max";
    return writeToFile(path, std::to_string(max_pids));
}
```

限制 cgroup 内同时存在的进程/线程总数。不同语言需要不同的进程数：

| 语言 | pids.max | 原因 |
|---|---|---|
| C/C++ | 10 | 编译型语言，单进程执行 |
| Python/JavaScript | 10 | 解释型语言，单进程执行 |
| Java | 100 | JVM 需要多个线程（GC、JIT 编译等） |
| C#/Mono | 50 | Mono 运行时需要更多线程 |

如果 fork 尝试会导致进程数超出 `pids.max` 限制，fork 会失败并返回 `EAGAIN`。

### 4. 资源使用监控

```cpp
size_t CgroupManager::getMemoryUsage() {
    std::string path = cgroupPath_ + "/memory.current";
    std::string content = readFromFile(path);
    if (content.empty()) return 0;
    return std::stoull(content);
}

long CgroupManager::getCpuUsage() {
    std::string path = cgroupPath_ + "/cpu.stat";
    std::string content = readFromFile(path);
    // 解析 usage_usec 字段
    ...
}
```

`memory.current` 和 `cpu.stat` 提供了实时的资源使用统计，用于返回给判题 API 的 `max_memory` 和 `max_time` 字段。

## 在代码执行流程中的应用

```cpp
// 在 executeInSandbox() 中：
CgroupManager cgroup(cgroupName);
cgroup.setMemoryLimit(limits.memory_bytes);  // 内存限制
cgroup.setPidsLimit(10);                      // 进程数限制

pid_t pid = fork();
if (pid == 0) {
    cgroup.addProcess(getpid());             // 加入 cgroup
    // ... setrlimit 设置 ...
    // ... seccomp 安装 ...
    execl(execPath.c_str(), execPath.c_str(), nullptr);
} else {
    // 等待子进程结束
    wait4(pid, &status, 0, &usage);
    result.memory = cgroup.getMemoryUsage(); // 获取内存使用量
    // ... 判定逻辑 ...
}
```

## Cgroup 目录结构示例

执行一次 C++ 判题时，cgroup 目录如下：

```
/sys/fs/cgroup/code_runner_<parent_pid>_<case_id>/
├── memory.max          ← 内存硬限制 (字节)
├── memory.current      ← 当前内存使用 (字节)
├── memory.events       ← 内存事件计数
├── memory.stat         ← 详细内存统计
├── cpu.max             ← CPU 使用配额
├── cpu.stat            ← CPU 使用统计
├── pids.max            ← 进程数限制
├── pids.current        ← 当前进程数
├── cgroup.procs        ← 进程列表
└── cgroup.events       ← cgroup 事件
```

## 故障排查

### 检查 cgroup v2 是否可用

```bash
# 查看挂载情况
mount | grep cgroup2

# 查看可用的控制器
cat /sys/fs/cgroup/cgroup.controllers
# 应该包含: cpu memory pids
```

### Cgroup 目录创建失败

```bash
# 确保有写入权限
ls -la /sys/fs/cgroup/
# 通常需要 root 权限才能创建子 cgroup
sudo ./code_runner
```

### 内存限制不生效

某些内核启动参数会影响 cgroup 内存控制器的行为：

- `cgroup.memory=nokmem` — 禁用内核内存统计（`memory.current` 始终为 0）
- `cgroup.memory=nosocket` — 禁用 socket 内存统计

在这些环境下，`memory.current` 可能返回 0，代码会自动退化为使用 `rusage.ru_maxrss`。

### 手动测试 cgroup

```bash
# 创建测试 cgroup
mkdir /sys/fs/cgroup/test_limit
echo "5242880" > /sys/fs/cgroup/test_limit/memory.max   # 5MB
echo "10" > /sys/fs/cgroup/test_limit/pids.max           # 10 进程

# 将当前 shell 加入（危险操作！）
echo $$ > /sys/fs/cgroup/test_limit/cgroup.procs

# 测试完成后务必移出
echo $$ > /sys/fs/cgroup/cgroup.procs
rmdir /sys/fs/cgroup/test_limit
```
