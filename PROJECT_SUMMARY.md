# 项目总结 - Code Runner

## ✅ 已完成功能

### 1. 核心架构
- ✅ C++ Web 服务器（基于 cpp-httplib）
- ✅ RESTful API 接口
- ✅ JSON 序列化/反序列化（使用 nlohmann/json）
- ✅ 多线程并发支持

### 2. 安全隔离
- ✅ **Seccomp**：系统调用过滤，限制危险操作
- ✅ **Namespace**：进程隔离（PID、IPC、UTS 等）
- ✅ **Cgroups**：资源限制（CPU、内存、进程数）

### 3. 语言支持
- ✅ C/C++（编译型）
- ✅ Python（解释型）
- ✅ Java（预留支持）
- ✅ Go（预留支持）
- ✅ 可配置的语言系统调用白名单

### 4. 资源管理
- ✅ CPU 时间限制
- ✅ 内存使用限制
- ✅ 栈空间限制
- ✅ 输出大小限制
- ✅ 进程数限制

### 5. 判定系统
- ✅ Accepted（通过）
- ✅ WrongAnswer（答案错误）
- ✅ TimeLimitExceeded（超时）
- ✅ MemoryLimitExceeded（内存超限）
- ✅ RuntimeError（运行错误）
- ✅ RestrictedSystemCall（非法系统调用）
- ✅ CompilationError（编译错误）
- ✅ SystemError（系统错误）

### 6. 测试与部署
- ✅ 完整的测试脚本
- ✅ Docker 支持
- ✅ Docker Compose 配置
- ✅ 快速启动脚本

## 📁 项目结构

```
sandbox-cpp/
├── CMakeLists.txt              # CMake 构建配置
├── Dockerfile                  # Docker 镜像定义
├── docker-compose.yml          # Docker Compose 配置
├── README.md                   # 完整文档
├── QUICKSTART.md               # 快速开始指南
├── .gitignore                  # Git 忽略文件
├── start.sh                    # 快速启动脚本
├── test.sh                     # 测试脚本
├── config/
│   └── languages.json          # 语言配置（C/C++/Python/Java/Go）
├── include/                    # 头文件
│   ├── cgroup_manager.h        # Cgroups 管理
│   ├── code_executor.h         # 代码执行引擎
│   ├── data_structures.h       # 数据结构定义
│   ├── namespace_isolation.h   # Namespace 隔离
│   ├── seccomp_filter.h        # Seccomp 过滤器
│   └── web_server.h            # Web 服务器
└── src/                        # 源文件
    ├── cgroup_manager.cpp      # Cgroups 实现
    ├── code_executor.cpp       # 执行引擎实现
    ├── data_structures.cpp     # 数据结构实现
    ├── main.cpp                # 主程序入口
    ├── namespace_isolation.cpp # Namespace 实现
    ├── seccomp_filter.cpp      # Seccomp 实现
    └── web_server.cpp          # Web 服务器实现
```

## 🔧 技术栈

- **语言**: C++17
- **构建工具**: CMake 3.15+
- **HTTP 库**: cpp-httplib
- **JSON 库**: nlohmann/json
- **安全**: libseccomp
- **系统**: Linux cgroups, namespace

## 📊 代码统计

- **头文件**: 6 个
- **源文件**: 7 个
- **总代码行数**: ~1500+ 行
- **配置文件**: 1 个（支持 5 种语言）

## 🎯 核心功能实现

### 1. Seccomp 过滤器 (seccomp_filter.cpp)
- 白名单模式系统调用过滤
- 支持自定义系统调用列表
- x86_64 架构系统调用映射

### 2. Namespace 隔离 (namespace_isolation.cpp)
- 创建独立的进程命名空间
- 文件系统隔离（chroot）
- 主机名隔离
- /proc 文件系统挂载

### 3. Cgroups 管理 (cgroup_manager.cpp)
- 支持 cgroup v2
- 内存限制
- CPU 时间限制
- 进程数限制
- 资源使用监控

### 4. 代码执行引擎 (code_executor.cpp)
- 编译型语言支持（C/C++）
- 解释型语言支持（Python）
- 多测试用例执行
- 完整的沙箱隔离
- 精确的资源统计

### 5. Web 服务器 (web_server.cpp)
- RESTful API
- JSON 请求/响应
- 异常处理
- 健康检查端点

## 🚀 API 端点

### POST /submit
提交代码执行请求

**请求体示例:**
```json
{
  "submission_id": 1,
  "language": "cpp",
  "code": "源代码",
  "test_cases": [...],
  "resources_limits": {
    "cpu_time": 1000,
    "memory_bytes": 268435456,
    "stack_bytes": 8388608,
    "output_bytes": 1048576
  }
}
```

### GET /health
服务健康检查

## 🔐 安全特性

1. **多层安全隔离**
   - Seccomp 系统调用过滤
   - Namespace 进程隔离
   - Cgroups 资源限制

2. **资源保护**
   - CPU 时间限制
   - 内存使用限制
   - 文件大小限制
   - 进程数限制

3. **系统调用白名单**
   - 每种语言独立配置
   - 最小权限原则
   - 禁止危险操作（网络、文件系统等）

## 📝 使用示例

### 编译和运行

```bash
# 编译
mkdir build && cd build
cmake .. && make

# 运行（需要 root 权限）
sudo ./code_runner
```

### 提交 C++ 代码

```bash
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 1,
    "language": "cpp",
    "code": "#include <iostream>\nint main() { std::cout << \"Hello\" << std::endl; }",
    "test_cases": [{"case_id": 1, "stdin": "", "expected": "Hello"}],
    "resources_limits": {
      "cpu_time": 1000,
      "memory_bytes": 268435456,
      "stack_bytes": 8388608,
      "output_bytes": 1048576
    }
  }'
```

## 🎓 设计亮点

1. **模块化设计**: 各组件职责清晰，易于维护和扩展
2. **安全优先**: 多层安全机制，确保系统安全
3. **性能优化**: C++ 实现，高性能代码执行
4. **可配置**: 语言配置独立，易于添加新语言
5. **容器化**: 完整的 Docker 支持

## ⚠️ 注意事项

1. **需要 root 权限**: cgroups 和 namespace 需要 root 权限
2. **Linux 专用**: 仅支持 Linux 系统
3. **内核要求**: Linux 内核 4.14+，推荐 5.0+
4. **cgroup v2**: 需要支持 cgroup v2 的系统

## 🔮 未来改进

1. **多用户隔离**: 使用不同的 UID/GID 运行代码
2. **网络隔离**: 添加网络命名空间隔离
3. **磁盘配额**: 限制磁盘使用
4. **编译缓存**: 缓存编译结果以提高性能
5. **更多语言**: 支持 Rust、JavaScript、Ruby 等
6. **并发控制**: 限制并发执行数量
7. **日志系统**: 完整的日志记录和审计
8. **监控告警**: 资源使用监控和告警

## 📞 联系方式

如有问题或建议，欢迎提交 Issue 或 Pull Request。

---

**项目创建时间**: 2025-12-26
**版本**: v1.0.0
**协议**: MIT License
