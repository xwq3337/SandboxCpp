# 沙箱虚拟系统构建 - 完整方案

## 快速开始

```bash
# 1. 构建虚拟系统 (从网络下载所有编译器)
sudo ./scripts/setup_sandbox_root.sh

# 2. 设置环境变量
source scripts/sandbox_env.sh

# 3. 编译并运行沙箱服务
cd build && make && ./sand-box-cpp
```

## 文件说明

| 文件 | 说明 |
|------|------|
| `scripts/setup_sandbox_root.sh` | 虚拟系统构建脚本 (网络安装版) |
| `scripts/sandbox_env.sh` | 环境变量设置脚本 (自动生成) |
| `config/sandbox_paths.json` | 编译器路径配置 (自动生成) |
| `config/sandbox_versions.txt` | 编译器版本信息 (自动生成) |
| `docs/SANDBOX_ROOT_SETUP.md` | 详细文档 |
| `docs/CODE_MODIFICATIONS.cpp` | 代码修改示例 |

## 核心特性

### 网络安装
脚本会从网络自动下载所有编译器，不依赖系统中已有的安装：

- **GCC/G++**: 从 apt 仓库安装
- **Python3**: 从 apt 仓库安装
- **Java (OpenJDK)**: 从 apt 仓库安装
- **Go**: 从官网下载 (支持华为云镜像备选)
- **Rust**: 使用 rustup 安装 (使用 rsproxy.cn 国内镜像)
- **Node.js**: 从 NodeSource 仓库安装
- **Zig**: 从官网下载
- **Mono (C#)**: 从 Mono 官方仓库安装

### 国内镜像支持
- **Rust**: 使用 `https://rsproxy.cn` 镜像
- **Go**: 配置 `GOPROXY=https://goproxy.cn,direct`

## 核心概念

构建一个统一的虚拟根目录，将所有编译器/解释器集中管理：

```
/opt/sandbox/               # 虚拟系统根目录
├── usr/bin/                # 所有编译器和解释器
│   ├── gcc                 # 从 apt 安装后复制
│   ├── g++
│   ├── python3
│   ├── java
│   ├── javac
│   ├── go                  # 从官网下载
│   ├── rustc               # 通过 rustup 安装
│   ├── cargo
│   ├── node
│   ├── npm
│   ├── zig
│   └── mcs
├── usr/local/              # 语言工具链
│   ├── go/                 # Go 完整发行版
│   ├── rustup/             # Rust 工具管理器
│   ├── cargo/              # Cargo 包管理器
│   └── zig/                # Zig 编译器
├── lib/                    # 必要的库文件
└── etc/                    # 配置文件
```

## 使用流程

### 1. 构建虚拟系统

```bash
# 使用默认路径 /opt/sandbox
sudo ./scripts/setup_sandbox_root.sh

# 或指定自定义路径
sudo ./scripts/setup_sandbox_root.sh /path/to/sandbox
```

脚本会执行以下步骤：
1. 更新 apt 包列表
2. 创建目录结构
3. 安装所有编译器/解释器
4. 复制到沙箱目录
5. 复制必要的库文件
6. 生成配置文件

### 2. 查看生成的配置

```bash
# 查看编译器路径
cat config/sandbox_paths.json

# 查看编译器版本
cat config/sandbox_versions.txt
```

### 3. 在代码中使用

#### 方法 A: 环境变量 (推荐)

```bash
# 启动服务前设置环境变量
export SANDBOX_ROOT=/opt/sandbox
./build/sand-box-cpp
```

#### 方法 B: 修改代码

参考 [docs/CODE_MODIFICATIONS.cpp](CODE_MODIFICATIONS.cpp)

## 安装的编译器

| 语言 | 编译器 | 版本 | 来源 |
|------|--------|------|------|
| C | gcc | 系统默认 | apt |
| C++ | g++ | 系统默认 | apt |
| Java | javac/java | OpenJDK default | apt |
| Python | python3 | 系统默认 | apt |
| Go | go | 1.23.4 | 官网下载 |
| Rust | rustc | stable (rustup) | rustup (rsproxy.cn) |
| Zig | zig | 0.13.0 | 官网下载 |
| Node.js | node/npm | 20 LTS | NodeSource |
| C# | mcs/mono | latest | Mono 官方仓库 |

## 目录结构

```
sand-box-cpp/
├── scripts/
│   └── setup_sandbox_root.sh     # 构建脚本
├── config/
│   ├── languages.json            # 语言配置
│   ├── sandbox_paths.json        # 生成的路径配置
│   └── sandbox_versions.txt      # 生成的版本信息
├── docs/
│   ├── SANDBOX_ROOT_SETUP.md     # 详细文档
│   ├── CODE_MODIFICATIONS.cpp    # 代码修改示例
│   └── SANDBOX_README.md         # 本文件
├── src/
│   └── code_executor.cpp         # 需要修改的源文件
└── include/
    └── code_executor.h           # 头文件
```

## 修改代码使用虚拟系统

### 修改 code_executor.cpp

在 `compile()` 函数中设置环境变量：

```cpp
// code_executor.cpp:158
pid_t compilePid = fork();
if (compilePid == 0) {
    // 获取沙箱根目录
    const char* sandboxRoot = getenv("SANDBOX_ROOT");
    if (!sandboxRoot) {
        sandboxRoot = "/opt/sandbox";
    }

    // 构建沙箱 PATH
    std::string sandboxBin = std::string(sandboxRoot) + "/usr/bin";
    std::string newPath = "PATH=" + sandboxBin + ":/bin:/usr/bin";
    setenv("PATH", newPath.c_str(), 1);

    // 执行编译命令
    ret = system(compileCmd.c_str());
    exit(ret != 0 ? 1 : 0);
}
```

在 `executeInterpreted()` 函数中使用沙箱解释器：

```cpp
// code_executor.cpp:461
pid_t pid = fork();
if (pid == 0) {
    // 获取沙箱根目录
    const char* sandboxRoot = getenv("SANDBOX_ROOT") ?: "/opt/sandbox";
    std::string sandboxBin = std::string(sandboxRoot) + "/usr/bin";

    // 使用沙箱中的解释器
    if (language == "python") {
        execl((sandboxBin + "/python3").c_str(), "python3", sourceFile.c_str(), nullptr);
    } else if (language == "javascript") {
        execl((sandboxBin + "/node").c_str(), "node", sourceFile.c_str(), nullptr);
    }
    // ...
}
```

## 测试

```bash
# 测试编译器是否可用
/opt/sandbox/usr/bin/gcc --version
/opt/sandbox/usr/bin/python3 --version
/opt/sandbox/usr/bin/go version
/opt/sandbox/usr/bin/rustc --version

# 查看所有版本
cat config/sandbox_versions.txt
```

## 故障排查

### 下载失败

如果某个编译器下载失败，脚本会继续安装其他编译器。可以单独安装：

```bash
# 重新运行脚本
sudo ./scripts/setup_sandbox_root.sh

# 或手动安装缺失的编译器
sudo apt install gcc g++ python3 default-jdk
```

### 库文件缺失

```bash
# 查看缺少的库
ldd /opt/sandbox/usr/bin/python3

# 手动复制库
sudo cp /path/to/lib.so /opt/sandbox/lib/x86_64-linux-gnu/
```

### 磁盘空间不足

脚本完成后会显示占用的磁盘空间。如果空间不足：

```bash
# 查看占用
du -sh /opt/sandbox

# 删除并重建
sudo rm -rf /opt/sandbox
sudo ./scripts/setup_sandbox_root.sh
```

## 高级配置

### 自定义安装路径

```bash
# 安装到自定义目录
sudo ./scripts/setup_sandbox_root.sh /home/myuser/sandbox

# 更新环境变量
export SANDBOX_ROOT=/home/myuser/sandbox
```

### 选择性安装

编辑 `scripts/setup_sandbox_root.sh`，注释掉不需要的部分：

```bash
# 如果不需要 Zig，注释掉这部分：
# ##############################################################################
# # 9. 安装 Zig
# ##############################################################################
# log_step "安装 Zig..."
# ...
```

### 使用代理

```bash
# 为 wget/curl 设置代理
export http_proxy=http://proxy.example.com:8080
export https_proxy=http://proxy.example.com:8080

sudo -E ./scripts/setup_sandbox_root.sh
# -E 参数保留环境变量
```

## 镜像源配置

### Rust 镜像

脚本默认使用 `https://rsproxy.cn` 国内镜像。如需修改：

```bash
# 编辑脚本中的环境变量
export RUSTUP_DIST_SERVER="https://rsproxy.cn"
export RUSTUP_UPDATE_ROOT="https://rsproxy.cn/rustup"
```

### Go 镜像

脚本默认配置了 `GOPROXY=https://goproxy.cn,direct`。

### APT 镜像

如果 apt 下载慢，可以修改 APT 源：

```bash
# 备份源列表
sudo cp /etc/apt/sources.list /etc/apt/sources.list.bak

# 使用国内镜像 (如阿里云)
# 编辑 /etc/apt/sources.list 后再运行脚本
sudo ./scripts/setup_sandbox_root.sh
```

## 与原有代码的兼容性

### 无需修改的情况

如果不需要完全隔离，可以直接使用环境变量：

```bash
export SANDBOX_ROOT=/opt/sandbox
export PATH="$SANDBOX_ROOT/usr/bin:$PATH"
```

这样代码中的编译命令会自动使用沙箱中的编译器。

### 需要修改的情况

如果需要完全隔离（使用 chroot），需要修改 [code_executor.cpp](../src/code_executor.cpp)。

参考 [docs/CODE_MODIFICATIONS.cpp](CODE_MODIFICATIONS.cpp) 中的详细示例。

## 注意事项

1. **需要 root 权限**: 脚本使用 apt 和复制系统文件，需要 sudo
2. **网络连接**: 需要稳定的网络连接下载编译器
3. **磁盘空间**: 完整安装需要约 3-5GB 空间
4. **安装时间**: 根据网络速度，可能需要 10-30 分钟
5. **用户检测**: 脚本自动检测当前用户（通过 SUDO_USER 或 logname）

## 相关文档

- [详细设置文档](SANDBOX_ROOT_SETUP.md)
- [代码修改示例](CODE_MODIFICATIONS.cpp)
- [languages.json 配置](../config/languages.json)

## 技术支持

如遇问题，请检查：
1. 网络连接是否正常
2. 磁盘空间是否充足
3. 是否有 root 权限
4. 查看生成的 `config/sandbox_versions.txt` 确认安装状态

## 许可

本项目遵循原项目的许可证。
