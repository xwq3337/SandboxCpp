# Namespace 进程隔离

## 概述

Linux Namespace 是内核提供的轻量级虚拟化特性，可以将进程的全局系统资源进行隔离，使进程认为自己拥有独立的系统实例。本项目的 `NamespaceIsolation` 类封装了 namespace 操作，提供进程隔离、文件系统隔离和主机名隔离能力。

源码文件：[src/namespace_isolation.cpp](../src/namespace_isolation.cpp) / [include/namespace_isolation.h](../include/namespace_isolation.h)

## Linux Namespace 类型

Linux 内核支持 8 种 namespace 类型：

| Namespace | 标志 | 隔离内容 |
|---|---|---|
| Mount | `CLONE_NEWNS` | 文件系统挂载点 |
| UTS | `CLONE_NEWUTS` | 主机名和域名 |
| IPC | `CLONE_NEWIPC` | System V IPC 和 POSIX 消息队列 |
| PID | `CLONE_NEWPID` | 进程 ID 编号空间 |
| Network | `CLONE_NEWNET` | 网络设备、IP 地址、路由表 |
| User | `CLONE_NEWUSER` | 用户和组 ID |
| Cgroup | `CLONE_NEWCGROUP` | Cgroup 根目录 |
| Time | `CLONE_NEWTIME` | 系统时间（Linux 5.6+） |

## 核心实现

### 类接口

```cpp
class NamespaceIsolation {
public:
    // 创建新的 namespace（使用 unshare 系统调用）
    static int createNamespaces(int flags);

    // 挂载 /proc 文件系统
    static int mountProc();

    // 设置主机名（需要 UTS namespace）
    static int setHostname(const std::string& hostname);

    // chroot 到指定目录（文件系统隔离）
    static int chrootToDir(const std::string& path);

    // 准备隔离环境（创建目录结构）
    static int prepareIsolatedEnv(const std::string& rootPath);
};
```

### 1. createNamespaces — 创建命名空间

```cpp
int NamespaceIsolation::createNamespaces(int flags) {
    if (unshare(flags) != 0) {
        fprintf(stderr, "Failed to create namespaces: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
```

通过 `unshare()` 系统调用将调用进程从当前的 namespace 中分离，创建新的独立命名空间。`flags` 参数按位组合指定要创建哪些 namespace。

**常用 flags 组合：**

```cpp
// 创建 PID + Mount + IPC + UTS namespace
int flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWIPC | CLONE_NEWUTS;
NamespaceIsolation::createNamespaces(flags);
```

### 2. mountProc — 挂载 /proc 文件系统

```cpp
int NamespaceIsolation::mountProc() {
    if (mount("proc", "/proc", "proc", 0, nullptr) != 0) {
        fprintf(stderr, "Failed to mount /proc: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
```

在新的 Mount namespace 中，需要重新挂载 `/proc` 以反映新 PID namespace 中的进程视图。在创建 `CLONE_NEWPID` 后，如果不重新挂载 `/proc`，进程看到的是父 namespace 的 `/proc` 内容，其中包含的 PID 信息将不正确。

### 3. setHostname — 主机名隔离

```cpp
int NamespaceIsolation::setHostname(const std::string& hostname) {
    if (sethostname(hostname.c_str(), hostname.length()) != 0) {
        fprintf(stderr, "Failed to set hostname: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
```

在 UTS namespace 中设置独立的主机名。这确保被隔离的进程看到的是沙箱特定的主机名，而不是宿主机的主机名。

### 4. chrootToDir — 文件系统根切换

```cpp
int NamespaceIsolation::chrootToDir(const std::string& path) {
    if (chroot(path.c_str()) != 0) {
        fprintf(stderr, "Failed to chroot to %s: %s\n", path.c_str(), strerror(errno));
        return -1;
    }
    if (chdir("/") != 0) {
        fprintf(stderr, "Failed to chdir to /: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}
```

`chroot` 将进程的根目录切换到指定路径。调用后，进程无法访问新根目录之外的文件系统。需要 root 权限执行。

**安全提示：** 仅靠 `chroot` 不能完全防止逃逸，在已有 root 权限的进程中可以通过多种方式突破 chroot。chroot 应作为多层隔离中的一层，而非唯一防线。

### 5. prepareIsolatedEnv — 准备隔离环境

```cpp
int NamespaceIsolation::prepareIsolatedEnv(const std::string& rootPath) {
    mkdir((rootPath + "/proc").c_str(), 0755);
    mkdir((rootPath + "/tmp").c_str(), 0755);
    mkdir((rootPath + "/dev").c_str(), 0755);
    return 0;
}
```

在新根文件系统中创建必要的目录结构，为后续的进程隔离做准备。

## 在本项目中的使用

当前实现中，`NamespaceIsolation` 提供了完整的 namespace 操作接口。在实际的代码执行流程（`code_executor.cpp`）中，隔离主要依赖以下机制：

1. **fork 创建子进程**：每次执行都在独立的子进程中运行
2. **chdir 切换工作目录**：切换到临时工作目录确保 CWD 有效
3. **setrlimit 资源限制**：与 cgroup 配合限制 CPU、内存、栈和输出大小
4. **cgroup 资源隔离**：通过 cgroup v2 限制内存、CPU 和进程数
5. **seccomp 系统调用过滤**：按需启用系统调用白名单

`NamespaceIsolation` 类提供了更进一步的隔离能力（PID/Mount/UTS namespace + chroot），可通过在 `executeInSandbox()` 等函数中调用 `createNamespaces()`、`mountProc()` 等方法实现更强的隔离。

## 隔离层级

```
HTTP 请求
    │
    ▼
WebServer (主进程)
    │
    ▼
CodeExecutor::execute()
    ├── compile() → fork + system()          ← 编译器运行在子进程中
    │
    └── runTestCase()
        └── executeInSandbox() → fork()
                ├── cgroup.addProcess()      ← 资源限制层
                ├── setrlimit(CPU/AS/STACK)  ← rlimit 资源限制层
                ├── seccomp (可选)            ← 系统调用过滤层
                ├── chdir(workDir)            ← 工作目录隔离
                └── execl()                   ← 执行用户程序
```

## 安全考量

1. **chroot 不是银弹**：root 进程可以逃逸 chroot，需要配合其他机制
2. **/proc 重新挂载**：创建 PID namespace 后必须重新挂载 /proc
3. **设备节点**：隔离环境中的 `/dev` 应只包含最小必要的设备节点
4. **分层防御**：namespace + cgroup + seccomp + rlimit 形成四层防御
