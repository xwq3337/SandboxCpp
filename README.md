# Code Runner — 多语言安全代码判题沙箱

基于 C++17 实现的安全代码执行与判题服务器，使用 Linux cgroup v2、namespace 和 seccomp 技术实现多层隔离的代码沙箱。

## 支持的语言

| 语言 | 类型 | 编译器/运行时 | 状态 |
|---|---|---|---|
| C++ | 编译型 | g++ -std=c++20 | 已测试 |
| C | 编译型 | gcc -std=c11 | 已配置 |
| Python | 解释型 | python3 | 已测试 |
| PyPy | 解释型 | pypy3 | 已配置 |
| Go | 编译型 | go build | 已测试 |
| Rust | 编译型 | rustc | 已测试 |
| Java | 编译型 | javac + java | 已配置 |
| JavaScript | 解释型 | node | 已配置 |
| C# | 编译型 | mcs + mono | 已配置 |
| Zig | 编译型 | zig build-exe | 已配置 |

## 判题结果 (Verdict)

| 判定 | 含义 | 触发条件 |
|---|---|---|
| `Accepted` | 通过 | 所有测试用例输出匹配 |
| `WrongAnswer` | 答案错误 | 输出与预期不匹配 |
| `TimeLimitExceeded` | 超时 | CPU 时间或墙上时间超限 |
| `MemoryLimitExceeded` | 内存超限 | 内存使用超过限制 |
| `RuntimeError` | 运行错误 | 非零退出码或异常信号 |
| `RestrictedSystemCall` | 非法系统调用 | 调用了白名单外的系统调用 |
| `CompilationError` | 编译错误 | 编译/语法错误 |
| `SystemError` | 系统错误 | 不支持的语言、fork 失败等内部错误 |

## 系统要求

- **操作系统**: Linux（内核 5.0+，支持 cgroup v2）
- **架构**: x86_64 或 aarch64 (ARM64)
- **编译器**: g++ 7+ (C++17)
- **构建工具**: CMake 3.15+
- **运行时依赖**: libseccomp-dev
- **权限**: root（cgroup 和 namespace 操作需要）

## 快速开始

### 1. 安装系统依赖

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libseccomp-dev
```

### 2. 构建

```bash
mkdir build && cd build
cmake .. && make -j$(nproc)
```

### 3. 运行

```bash
sudo ./code_runner           # 默认端口 8080
sudo ./code_runner 9000      # 指定端口
```

### 4. 测试

```bash
# 运行全部单元测试
./test/code_runner_tests

# 按过滤器运行
./test/code_runner_tests --gtest_filter="VerdictTest.*"
./test/code_runner_tests --gtest_filter="LanguagesTest.TestPython"

# 使用 ctest
ctest --output-on-failure
```

## API 接口

### `GET /health`
健康检查。

**响应：**
```json
{"status": "healthy", "service": "code_runner"}
```

### `POST /submit`
提交代码执行。

**请求体：**
```json
{
  "submission_id": 12345,
  "language": "cpp",
  "code": "#include <iostream>\nusing namespace std;\nint main() {\n    int a, b;\n    cin >> a >> b;\n    cout << a + b << endl;\n    return 0;\n}",
  "test_cases": [
    {"case_id": 1, "stdin": "1 2", "expected": "3"},
    {"case_id": 2, "stdin": "3 4", "expected": "7"}
  ],
  "resources_limits": {
    "cpu_time": 1000,
    "memory_bytes": 268435456,
    "stack_bytes": 8388608,
    "output_bytes": 1048576
  },
  "message": "",
  "seccomp_profile": ""
}
```

**字段说明：**

| 字段 | 类型 | 说明 |
|---|---|---|
| `submission_id` | int64 | 提交编号 |
| `language` | string | 语言标识（cpp/c/python/go/rust/java/javascript/csharp/zig/pypy） |
| `code` | string | 源代码 |
| `test_cases` | array | 测试用例列表 |
| `test_cases[].case_id` | int | 用例编号 |
| `test_cases[].stdin` | string | 标准输入 |
| `test_cases[].expected` | string | 期望输出 |
| `resources_limits.cpu_time` | int | CPU 时间限制（毫秒） |
| `resources_limits.memory_bytes` | int | 内存限制（字节） |
| `resources_limits.stack_bytes` | int | 栈大小限制（字节） |
| `resources_limits.output_bytes` | int | 输出大小限制（字节） |
| `seccomp_profile` | string | 自定义 seccomp 白名单（JSON 数组），为空则不启用 seccomp |

**响应：**
```json
{
  "submission_id": 12345,
  "language": "cpp",
  "code": "...",
  "verdict": "Accepted",
  "max_time": 15,
  "max_memory": 2048000,
  "result": [
    {
      "case_id": "1",
      "stdin": "1 2",
      "stdout": "3",
      "stderr": "",
      "status": "Accepted",
      "time": 15,
      "memory": 2048000,
      "expected": "3"
    }
  ]
}
```

### `GET /stats`
运行统计。返回全局吞吐量、按语言统计、判定结果分布等。

## 安全架构

项目采用四层沙箱隔离：

```
HTTP Request
    │
    ▼
┌─────────────────────────────────────┐
│  1. fork() 子进程                   │  ← 进程隔离
│     └── chdir(临时工作目录)          │  ← 目录隔离
├─────────────────────────────────────┤
│  2. setrlimit()                     │  ← 资源限制
│     ├── RLIMIT_CPU   (CPU 时间)     │
│     ├── RLIMIT_AS    (地址空间)     │
│     ├── RLIMIT_STACK (栈大小)       │
│     └── RLIMIT_FSIZE (输出大小)     │
├─────────────────────────────────────┤
│  3. cgroup v2                       │  ← 资源隔离
│     ├── memory.max  (内存限制)      │
│     ├── cpu.max     (CPU 配额)      │
│     └── pids.max    (进程数限制)    │
├─────────────────────────────────────┤
│  4. seccomp (按需启用)              │  ← 系统调用过滤
│     └── 白名单模式的系统调用控制    │
└─────────────────────────────────────┘
    │
    ▼
execl() 执行用户程序
```

## 项目结构

```
sandbox-cpp/
├── CMakeLists.txt                 # CMake 构建配置
├── Dockerfile                     # Docker 镜像
├── docker-compose.yml             # Docker Compose 配置
├── start.sh                       # 快速启动脚本
├── README.md
├── config/
│   ├── languages.json             # 10 种语言的编译/运行配置和系统调用白名单
│   └── sandbox_paths.json         # 沙箱路径配置
├── include/                       # 头文件
│   ├── data_structures.h          # 数据结构 (InputStruct, OutputResult, Verdict 等)
│   ├── code_executor.h            # 代码执行引擎
│   ├── compile_cache.h            # 编译缓存
│   ├── cgroup_manager.h           # Cgroup v2 资源管理
│   ├── namespace_isolation.h      # Namespace 进程隔离
│   ├── seccomp_filter.h           # Seccomp 系统调用过滤
│   ├── stats_collector.h          # 统计收集器
│   └── web_server.h               # HTTP Web 服务器
├── src/                           # 源文件
│   ├── main.cpp                   # 入口
│   ├── code_executor.cpp          # 执行引擎（编译、沙箱运行、判定）
│   ├── compile_cache.cpp          # 编译缓存实现 (FNV-1a hash)
│   ├── cgroup_manager.cpp         # Cgroup v2 操作
│   ├── namespace_isolation.cpp    # Namespace 操作
│   ├── seccomp_filter.cpp         # Seccomp 过滤
│   ├── stats_collector.cpp        # 统计收集
│   ├── web_server.cpp             # HTTP 路由和处理
│   └── data_structures.cpp        # 数据结构
├── test/                          # 单元测试
│   ├── CMakeLists.txt
│   ├── test_data_structures.cpp   # 数据结构序列化测试
│   ├── test_languages.cpp         # 各语言执行测试
│   └── test_judge_verdict.cpp     # 判定结果测试 (8 种 Verdict)
├── scripts/                       # 辅助脚本
│   ├── setup_sandbox_root.sh      # 虚拟根文件系统构建
│   └── sandbox_env.sh             # 沙箱环境变量
└── docs/                          # 文档
    ├── PROJECT_SUMMARY.md
    ├── QUICKSTART.md
    ├── SECCOMP.md                 # Seccomp 系统调用拦截
    ├── NAMESPACES.md              # Namespace 进程隔离
    ├── CGROUPS.md                 # Cgroups 资源管理
    └── SANDBOX_ROOT_SETUP.md      # 虚拟根文件系统构建
```

## 核心特性

### 编译缓存
基于 FNV-1a 64-bit hash 的编译缓存机制。key = `hash(language + code + compile_cmd)`，命中则跳过编译，直接复用产物。LRU 淘汰策略，默认最大 500 条。

### 统计系统
`GET /stats` 端点提供：
- 运行时长、总提交数、吞吐量
- 全局和按语言的平均执行时间
- 判定结果分布（Accepted / WrongAnswer / TLE / ... 计数）

### 多测试用例
一次提交支持多个测试用例，每个用例独立运行并返回各自的判定状态。整体 verdict 取第一个非 Accepted 的结果。

## 配置

### 语言配置 (`config/languages.json`)

```json
{
  "cpp": {
    "compile_cmd": "g++ -Wall -Wextra -O2 -std=c++20 -o {output} {source} -lm",
    "run_cmd": "{output}",
    "source_file": "main.cpp",
    "allow_sys_calls": ["read", "write", "close", ...]
  }
}
```

占位符：
- `{source}` — 源代码文件路径
- `{output}` — 编译产物路径
- `{memory}` — 内存限制（Java 的 -Xmx）
- `{stack}` — 栈限制（Java 的 -Xss）

### 添加新测试

在 `test/` 目录下创建 `.cpp` 文件，然后在 `test/CMakeLists.txt` 的 `add_executable` 中加入对应源文件即可。

## Docker 部署

```bash
docker build -t code-runner .
docker run --privileged -p 8080:8080 code-runner

# 或使用 docker-compose
docker-compose up -d
```

## 技术栈

- **语言**: C++17
- **构建**: CMake 3.15+
- **HTTP**: cpp-httplib v0.14.3
- **JSON**: nlohmann/json
- **安全**: libseccomp
- **测试**: Google Test v1.14.0
- **容器**: Docker + Docker Compose

## 文档索引

| 文档 | 内容 |
|---|---|
| [QUICKSTART.md](docs/QUICKSTART.md) | 快速开始指南 |
| [PROJECT_SUMMARY.md](docs/PROJECT_SUMMARY.md) | 项目总结 |
| [SECCOMP.md](docs/SECCOMP.md) | Seccomp 系统调用拦截原理与配置 |
| [NAMESPACES.md](docs/NAMESPACES.md) | Namespace 进程隔离原理与配置 |
| [CGROUPS.md](docs/CGROUPS.md) | Cgroup v2 资源管理原理与配置 |
| [SANDBOX_ROOT_SETUP.md](docs/SANDBOX_ROOT_SETUP.md) | 虚拟 Linux 根文件系统构建 |

## License

MIT
