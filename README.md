# Code Runner - 安全的代码执行服务器

一个基于 C++ 实现的安全代码执行服务器，使用 Linux cgroups、namespace 和 seccomp 技术来隔离和限制代码执行。

## 功能特性

- ✅ **多语言支持**：C、C++、Python、Java、Go
- 🔒 **安全隔离**：使用 namespace 隔离进程
- 🛡️ **系统调用过滤**：通过 seccomp 限制危险的系统调用
- 📊 **资源限制**：使用 cgroups 限制 CPU、内存等资源
- 🌐 **HTTP API**：提供 RESTful API 接口
- ⚡ **高性能**：C++ 实现，性能优异

## 系统要求

- Linux 系统（内核 4.14+，推荐 5.0+）
- CMake 3.15+
- GCC 7+ 或 Clang 6+
- libseccomp-dev
- 编译器：g++、gcc、python3、javac、go（根据需要支持的语言）

## 安装依赖

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libseccomp-dev \
    g++ \
    python3 \
    default-jdk \
    golang
```

### CentOS/RHEL

```bash
sudo yum install -y \
    gcc-c++ \
    cmake \
    libseccomp-devel \
    python3 \
    java-devel \
    golang
```

## 编译构建

```bash
# 创建 build 目录
mkdir build
cd build

# 配置和编译
cmake ..
make -j$(nproc)

# 安装（可选）
sudo make install
```

## 运行服务器

```bash
# 直接运行（需要 root 权限以使用 cgroups）
sudo ./code_runner

# 指定端口
sudo ./code_runner 8080

# 或者从 build 目录
cd build
sudo ./code_runner
```

服务器默认监听 `0.0.0.0:8080`。

## API 使用说明

### 1. 健康检查

```bash
GET /health
```

**响应示例：**
```json
{
  "status": "healthy",
  "service": "code_runner"
}
```

### 2. 提交代码执行

```bash
POST /submit
Content-Type: application/json
```

**请求体：**
```json
{
  "submission_id": 12345,
  "language": "cpp",
  "code": "#include <iostream>\nint main() {\n    std::cout << \"Hello World\" << std::endl;\n    return 0;\n}",
  "test_cases": [
    {
      "case_id": 1,
      "stdin_data": "",
      "expected": "Hello World"
    }
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

**响应示例：**
```json
{
  "submission_id": 12345,
  "language": "cpp",
  "code": "#include <iostream>\nint main() {\n    std::cout << \"Hello World\" << std::endl;\n    return 0;\n}",
  "verdict": "Accepted",
  "max_time": 15,
  "max_memory": 2048000,
  "result": [
    {
      "case_id": "1",
      "stdin_data": "",
      "stdout_data": "Hello World",
      "stderr_data": "",
      "status": "Accepted",
      "time": 15,
      "memory": 2048000,
      "expected": "Hello World"
    }
  ]
}
```

## 判定结果说明

- `Accepted`：通过所有测试
- `WrongAnswer`：输出不匹配
- `TimeLimitExceeded`：超过时间限制
- `MemoryLimitExceeded`：超过内存限制
- `RuntimeError`：运行时错误
- `RestrictedSystemCall`：使用了被禁止的系统调用
- `CompilationError`：编译错误
- `SystemError`：系统错误

## 测试示例

### C++ 示例

```bash
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 1,
    "language": "cpp",
    "code": "#include <iostream>\nint main() {\n    int a, b;\n    std::cin >> a >> b;\n    std::cout << a + b << std::endl;\n    return 0;\n}",
    "test_cases": [
      {
        "case_id": 1,
        "stdin_data": "1 2",
        "expected": "3"
      }
    ],
    "resources_limits": {
      "cpu_time": 1000,
      "memory_bytes": 268435456,
      "stack_bytes": 8388608,
      "output_bytes": 1048576
    },
    "message": "",
    "seccomp_profile": ""
  }'
```

### Python 示例

```bash
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 2,
    "language": "python",
    "code": "a, b = map(int, input().split())\nprint(a + b)",
    "test_cases": [
      {
        "case_id": 1,
        "stdin_data": "1 2",
        "expected": "3"
      }
    ],
    "resources_limits": {
      "cpu_time": 2000,
      "memory_bytes": 268435456,
      "stack_bytes": 8388608,
      "output_bytes": 1048576
    },
    "message": "",
    "seccomp_profile": ""
  }'
```

## 语言配置

语言配置文件位于 `config/languages.json`，可以自定义编译和运行命令，以及允许的系统调用。

**配置示例：**
```json
{
  "cpp": {
    "compile_cmd": "g++ -O2 -std=c++17 -o {output} {source}",
    "run_cmd": "{output}",
    "allow_sys_calls": [
      "read", "write", "close", "exit", ...
    ]
  }
}
```

## 安全注意事项

1. **Root 权限**：服务器需要 root 权限才能使用 cgroups 和 namespace
2. **防火墙**：建议使用防火墙限制访问
3. **资源限制**：根据服务器性能调整资源限制
4. **系统调用白名单**：谨慎配置允许的系统调用
5. **网络隔离**：建议在隔离的网络环境中运行

## 故障排除

### 问题：无法创建 cgroup

**解决方案：**
```bash
# 确保 cgroup v2 已挂载
sudo mount -t cgroup2 none /sys/fs/cgroup

# 检查 cgroup 是否可用
ls /sys/fs/cgroup/
```

### 问题：Seccomp 过滤器安装失败

**解决方案：**
```bash
# 检查内核是否支持 seccomp
grep CONFIG_SECCOMP /boot/config-$(uname -r)

# 安装 libseccomp
sudo apt-get install libseccomp-dev
```

### 问题：编译失败

**解决方案：**
```bash
# 清理并重新编译
rm -rf build
mkdir build
cd build
cmake ..
make VERBOSE=1
```

## 项目结构

```
sandbox-cpp/
├── CMakeLists.txt          # CMake 配置
├── README.md               # 项目文档
├── config/                 # 配置文件
│   └── languages.json      # 语言配置
├── include/                # 头文件
│   ├── cgroup_manager.h
│   ├── code_executor.h
│   ├── data_structures.h
│   ├── namespace_isolation.h
│   ├── seccomp_filter.h
│   └── web_server.h
└── src/                    # 源文件
    ├── cgroup_manager.cpp
    ├── code_executor.cpp
    ├── data_structures.cpp
    ├── main.cpp
    ├── namespace_isolation.cpp
    ├── seccomp_filter.cpp
    └── web_server.cpp
```

## 技术架构

### 核心组件

1. **Web Server**：基于 cpp-httplib 的 HTTP 服务器
2. **Code Executor**：代码执行引擎
3. **Seccomp Filter**：系统调用过滤
4. **Namespace Isolation**：进程隔离
5. **Cgroup Manager**：资源限制管理

### 执行流程

```
HTTP Request → Parse JSON → Compile Code → Execute in Sandbox
                                              ↓
                                         ┌────────────┐
                                         │  Namespace │
                                         │  + Cgroup  │
                                         │  + Seccomp │
                                         └────────────┘
                                              ↓
                                        Collect Results → Response
```

## 性能优化建议

1. 使用编译缓存减少重复编译
2. 调整 cgroup 限制以匹配服务器性能
3. 使用连接池处理并发请求
4. 定期清理临时文件

## 贡献

欢迎提交 Issue 和 Pull Request！

## 许可证

MIT License

## 作者

Your Name

## 更新日志

### v1.0.0 (2025-12-26)
- 初始版本发布
- 支持 C、C++、Python、Java、Go
- 实现 cgroups、namespace、seccomp 安全隔离
