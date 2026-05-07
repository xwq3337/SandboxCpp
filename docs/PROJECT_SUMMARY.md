# 项目总结 — Code Runner

## 项目概述

Code Runner 是一个基于 C++17 的多语言安全代码判题沙箱。通过 Linux cgroup v2、namespace 和 seccomp 技术实现四层沙箱隔离，支持 10 种编程语言的编译运行和自动判题。

## 当前版本状态

- **版本**: v1.1.0
- **C++ 标准**: C++17
- **测试用例**: 19 个（数据结构 + 语言执行 + 判定覆盖）
- **支持语言**: 10 种
- **架构支持**: x86_64 / aarch64

## 已完成的特性

### 1. 多语言支持（10 种）

| 语言 | 类型 | 状态 |
|---|---|---|
| C++ (g++ -std=c++20) | 编译型 | 已验证 |
| C (gcc -std=c11) | 编译型 | 已配置 |
| Python 3 | 解释型 | 已验证 |
| PyPy 3 | 解释型 | 已配置 |
| Go | 编译型 | 已验证 |
| Rust | 编译型 | 已验证 |
| Java 17 | 编译型 | 已配置 |
| JavaScript (Node.js) | 解释型 | 已配置 |
| C# (Mono) | 编译型 | 已配置 |
| Zig | 编译型 | 已配置 |

### 2. 安全隔离（四层沙箱）

| 层级 | 机制 | 实现文件 |
|---|---|---|
| 进程隔离 | fork() + chdir() | `code_executor.cpp` |
| 资源限制 | setrlimit (CPU/Stack/AS/FSIZE) | `code_executor.cpp` |
| 资源隔离 | cgroup v2 (memory/cpu/pids) | `cgroup_manager.cpp` |
| 系统调用过滤 | seccomp (白名单模式) | `seccomp_filter.cpp` |

### 3. 判题系统（8 种 Verdict）

`Accepted`, `WrongAnswer`, `TimeLimitExceeded`, `MemoryLimitExceeded`, `RuntimeError`, `RestrictedSystemCall`, `CompilationError`, `SystemError`

### 4. 编译缓存

基于 FNV-1a 64-bit hash 的编译产物缓存，key 为 `hash(language + code + compile_cmd)`，LRU 淘汰。

### 5. 统计系统

- 全局：运行时长、总提交数、吞吐量
- 按语言：提交数、平均时间
- 判定分布：各 Verdict 计数

### 6. HTTP API

| 端点 | 方法 | 说明 |
|---|---|---|
| `/submit` | POST | 提交代码执行 |
| `/health` | GET | 健康检查 |
| `/stats` | GET | 运行统计 |

### 7. 测试覆盖

| 测试文件 | 用例数 | 覆盖范围 |
|---|---|---|
| `test_data_structures.cpp` | 7 | JSON 序列化/反序列化、InputStruct 解析、OutputResult 序列化 |
| `test_languages.cpp` | 4 | C++/Python/Go/Rust 语言正确性测试 |
| `test_judge_verdict.cpp` | 8 | Accepted/WrongAnswer/CompilationError/RuntimeError/TimeLimitExceeded/MemoryLimitExceeded/RestrictedSystemCall/MultipleTestCases |

## 核心文件清单

```
include/
├── data_structures.h       # 数据结构 (Verdict, InputStruct, OutputResult 等)
├── code_executor.h         # 代码执行引擎
├── compile_cache.h         # 编译缓存
├── cgroup_manager.h        # Cgroup v2 管理
├── namespace_isolation.h   # Namespace 隔离
├── seccomp_filter.h        # Seccomp 系统调用过滤
├── stats_collector.h       # 统计收集
├── web_server.h            # HTTP 服务器
└── json.hpp                # nlohmann/json 单头文件

src/
├── main.cpp                # 入口
├── code_executor.cpp       # 核心执行逻辑 (~890 行)
├── cgroup_manager.cpp      # Cgroup v2 操作 (~120 行)
├── namespace_isolation.cpp # Namespace 操作 (~65 行)
├── seccomp_filter.cpp      # Seccomp 过滤 (~150 行)
├── compile_cache.cpp       # 编译缓存 (~110 行)
├── stats_collector.cpp     # 统计收集 (~85 行)
├── web_server.cpp          # HTTP 路由 (~80 行)
└── data_structures.cpp     # 数据结构

test/
├── test_data_structures.cpp    # 7 个数据结构测试
├── test_languages.cpp          # 4 个语言执行测试
└── test_judge_verdict.cpp      # 8 个判定系统测试
```

## 设计亮点

1. **模块化架构**：各组件（CgroupManager, SeccompFilter, NamespaceIsolation, StatsCollector, CompileCache）职责清晰，可独立测试和复用
2. **四层防御深度**：fork + rlimit + cgroup + seccomp 形成纵深防御
3. **按需安全策略**：seccomp 通过 `seccomp_profile` 字段按需启用，不强制所有请求都承受过滤开销
4. **多架构支持**：同时支持 x86_64 和 aarch64，预编译工具链已包含在 `scripts/` 中
5. **统一沙箱路径**：所有编译器/解释器统一到 `/opt/sandbox/usr/bin/`，解决路径分散问题
6. **编译缓存**：FNV-1a hash + LRU 淘汰，减少重复编译开销

## 技术债务与改进方向

1. **Seccomp 默认启用**：当前 seccomp 仅按需启用，可考虑默认启用并逐步完善各语言白名单
2. **Namespace 深度集成**：`NamespaceIsolation` 接口已就绪，可进一步集成 PID/Mount namespace
3. **网络隔离**：可添加 `CLONE_NEWNET` 创建独立网络栈
4. **磁盘配额**：限制临时文件大小和工作目录空间
5. **并发控制**：限制同时执行的判题任务数
6. **日志与审计**：结构化日志记录每次判题详情
7. **更多语言**：TypeScript/Deno、Ruby、PHP、Swift 等
