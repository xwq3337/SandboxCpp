# 快速开始指南

## 方式一：直接编译运行

### 1. 安装依赖

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libseccomp-dev g++ python3
```

### 2. 编译项目

```bash
cd /home/ubuntu/sandbox-cpp
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### 3. 运行服务器

```bash
# 需要 root 权限
sudo ./code_runner
```

或者使用快速启动脚本：

```bash
sudo ./start.sh
```

## 方式二：使用 Docker

### 1. 构建镜像

```bash
docker build -t code-runner .
```

### 2. 运行容器

```bash
docker run --privileged -p 8080:8080 code-runner
```

或者使用 docker-compose：

```bash
docker-compose up -d
```

## 测试服务

### 1. 健康检查

```bash
curl http://localhost:8080/health
```

### 2. 运行测试脚本

```bash
./test.sh
```

### 3. 手动测试

```bash
curl -X POST http://localhost:8080/submit \
  -H "Content-Type: application/json" \
  -d '{
    "submission_id": 1,
    "language": "cpp",
    "code": "#include <iostream>\nint main() { std::cout << \"Hello World\" << std::endl; return 0; }",
    "test_cases": [{
      "case_id": 1,
      "stdin_data": "",
      "expected": "Hello World"
    }],
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

## 常见问题

### Q: 为什么需要 root 权限？

A: 服务器使用 Linux cgroups 和 namespace 进行隔离，这些功能需要 root 权限。

### Q: 如何添加新的编程语言支持？

A: 编辑 `config/languages.json` 文件，添加语言配置。

### Q: 如何调整资源限制？

A: 在请求中修改 `resources_limits` 字段的值。

### Q: 编译失败怎么办？

A: 确保已安装所有依赖，特别是 libseccomp-dev。

## 下一步

- 阅读完整的 [README.md](README.md)
- 查看 API 文档
- 配置语言支持
- 部署到生产环境
