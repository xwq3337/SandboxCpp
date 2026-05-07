# 虚拟 Linux 根文件系统构建指南

## 概述

为代码执行沙箱构建一个统一的虚拟 Linux 根文件系统，将所有编译器/解释器/运行时集中到 `/opt/sandbox/` 目录下，解决多语言工具链路径分散、环境依赖不一致的问题。

## 设计目标

1. **统一路径**：所有编译器、解释器、运行时固定于 `/opt/sandbox/usr/bin/`
2. **库依赖完整**：复制必要的共享库到 `/opt/sandbox/usr/lib/`、`/opt/sandbox/lib/`
3. **可移植**：整个目录可复制到其他同架构 Linux 系统直接使用
4. **与 host 隔离**：通过受控的 PATH 和 LD_LIBRARY_PATH 指向沙箱目录

## 目录结构

```
/opt/sandbox/
├── bin/                    # 基本工具 (符号链接)
│   ├── bash -> /bin/bash
│   └── sh -> /bin/sh
├── usr/
│   ├── bin/                # 编译器、解释器 (符号链接)
│   │   ├── gcc -> /usr/bin/gcc
│   │   ├── g++ -> /usr/bin/g++
│   │   ├── g++-13 -> /usr/bin/g++-13
│   │   ├── aarch64-linux-gnu-g++-13 -> /usr/bin/aarch64-linux-gnu-g++-13
│   │   ├── cc1plus          (从 g++ 依赖复制)
│   │   ├── cc1              (从 gcc 依赖复制)
│   │   ├── python3 -> /usr/bin/python3
│   │   ├── pypy3 -> /usr/bin/pypy3
│   │   ├── node -> /usr/local/node/bin/node
│   │   ├── java -> /usr/bin/java
│   │   ├── javac -> /usr/bin/javac
│   │   ├── go -> /usr/local/go/bin/go
│   │   ├── rustc -> ~/.cargo/bin/rustc
│   │   ├── zig -> ~/.zig/zig
│   │   ├── mcs -> /usr/bin/mcs
│   │   └── mono -> /usr/bin/mono
│   ├── lib/                # 共享库
│   │   ├── aarch64-linux-gnu/
│   │   │   ├── libc.so.6
│   │   │   ├── libm.so.6
│   │   │   ├── libstdc++.so.6
│   │   │   └── ...
│   │   └── ...
│   ├── lib64/
│   │   └── ld-linux-aarch64.so.1
│   ├── local/
│   │   ├── go/             # Go 工具链
│   │   ├── node/           # Node.js 运行时
│   │   ├── zig/            # Zig 编译器
│   │   ├── cargo/          # Rust 包管理器
│   │   └── rustup/         # Rust 工具链管理器
│   └── share/              # 共享数据文件
├── etc/                    # 基本配置文件
│   ├── passwd
│   ├── group
│   └── hosts
├── home/
│   └── ubuntu/             # HOME 目录
├── tmp/                    # 临时文件
└── proc/                   # proc 挂载点
```

## 在当前代码中的使用

项目代码中已经硬编码了沙箱路径。主要在以下位置：

### 编译阶段 (`code_executor.cpp` — `compile()`)

```cpp
// PATH 设置（fork 后编译子进程中）
const char* newPath = "/opt/sandbox/usr/bin:"
    "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:"
    "/opt/sandbox/usr/local/go/bin:"
    "/opt/sandbox/usr/local/node/bin:"
    "/opt/sandbox/usr/local/zig:"
    "/opt/sandbox/usr/local/cargo/bin";

// 库搜索路径
const char* newLibPath = "/opt/sandbox/usr/lib:"
    "/opt/sandbox/lib:/opt/sandbox/lib64:"
    "/opt/sandbox/usr/local/lib";

// 环境变量
setenv("RUSTUP_HOME", "/opt/sandbox/usr/local/rustup", 1);
setenv("CARGO_HOME", "/opt/sandbox/usr/local/cargo", 1);
```

### 执行阶段 (`code_executor.cpp` — `executeInterpreted()`)

```cpp
// 解释型语言使用沙箱中的解释器
if (language == "python") {
    execl("/opt/sandbox/usr/bin/python3", "python3", sourceFile.c_str(), nullptr);
} else if (language == "javascript") {
    execl("/opt/sandbox/usr/bin/node", "node", sourceFile.c_str(), nullptr);
} else if (language == "pypy") {
    execl("/opt/sandbox/usr/bin/pypy3", "pypy3", sourceFile.c_str(), nullptr);
}
```

### C#/Mono 执行 (`executeCSharp()`)

```cpp
const char* newPath = "/opt/sandbox/usr/bin:/usr/local/sbin:/usr/local/bin:"
    "/usr/sbin:/usr/bin:/sbin:/bin";
setenv("PATH", newPath, 1);
execl("/opt/sandbox/usr/bin/mono", "mono", execPath.c_str(), nullptr);
```

### Java 执行 (`executeJava()`)

```cpp
execl("/opt/sandbox/usr/bin/java", "java", "Main", nullptr);
```

## 构建步骤

### 使用自动化脚本

```bash
# 使用默认路径 /opt/sandbox
sudo ./scripts/setup_sandbox_root.sh

# 或指定自定义路径
sudo ./scripts/setup_sandbox_root.sh /path/to/sandbox
```

### 手动构建

```bash
# 1. 创建目录结构
SANDBOX=/opt/sandbox
sudo mkdir -p $SANDBOX/{bin,usr/{bin,lib,local/{go,node,zig,cargo,rustup}},lib64,etc,home/ubuntu,tmp,proc}

# 2. 创建编译器/解释器符号链接
sudo ln -sf /usr/bin/gcc $SANDBOX/usr/bin/gcc
sudo ln -sf /usr/bin/g++ $SANDBOX/usr/bin/g++
sudo ln -sf /usr/bin/python3 $SANDBOX/usr/bin/python3
sudo ln -sf /usr/bin/java $SANDBOX/usr/bin/java
# ... 其他语言

# 3. 复制必要的共享库
sudo cp -r /lib/aarch64-linux-gnu $SANDBOX/lib/
sudo cp /lib64/ld-linux-aarch64.so.1 $SANDBOX/lib64/

# 4. 复制编译器内部工具
GCC_LIB=$(g++ -print-search-dirs | grep install | cut -d' ' -f2)
sudo cp $GCC_LIB/cc1plus $SANDBOX/usr/bin/
sudo cp $GCC_LIB/cc1 $SANDBOX/usr/bin/

# 5. 创建基本配置文件
echo "root:x:0:0:root:/root:/bin/bash" | sudo tee $SANDBOX/etc/passwd
echo "root:x:0:" | sudo tee $SANDBOX/etc/group
echo "127.0.0.1 localhost" | sudo tee $SANDBOX/etc/hosts

# 6. 设置目录权限
sudo chmod 1777 $SANDBOX/tmp
sudo chmod 755 $SANDBOX/usr/bin
```

## 验证构建

```bash
# 验证编译器可用
PATH=/opt/sandbox/usr/bin:$PATH g++ --version

# 验证解释器可用
/opt/sandbox/usr/bin/python3 --version

# 验证库依赖
ldd /opt/sandbox/usr/bin/g++
```

## 支持的架构

当前沙箱支持两种 CPU 架构：

| 架构 | 预编译工具包 |
|---|---|
| x86_64 | `scripts/zig-x86_64-linux-*.tar.xz`, `scripts/node-v*-linux-x64.tar.xz`, `scripts/go*.linux-amd64.tar.gz` |
| aarch64 (ARM64) | `scripts/zig-aarch64-linux-*.tar.xz`, `scripts/node-v*-linux-arm64.tar.xz`, `scripts/go*.linux-arm64.tar.gz` |

## 故障排查

### 编译器找不到 cc1plus / cc1

```bash
# 查找 cc1plus 路径
g++ -print-search-dirs | grep install
# 复制到沙箱
sudo cp $(g++ -print-search-dirs | grep install | cut -d' ' -f2)/cc1plus /opt/sandbox/usr/bin/
sudo cp $(g++ -print-search-dirs | grep install | cut -d' ' -f2)/cc1 /opt/sandbox/usr/bin/
```

### 库文件缺失

```bash
# 查看可执行文件依赖
ldd /opt/sandbox/usr/bin/g++

# 手动复制缺失的库
sudo cp /lib/aarch64-linux-gnu/libxxx.so /opt/sandbox/lib/aarch64-linux-gnu/
```

### 解释器找不到

```bash
# 检查符号链接目标
readlink -f /opt/sandbox/usr/bin/python3

# 若目标不存在，重新创建符号链接
sudo ln -sf $(which python3) /opt/sandbox/usr/bin/python3
```
