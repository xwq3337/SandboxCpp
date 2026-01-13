# 沙箱虚拟系统构建指南

## 概述

本文档说明如何为代码执行沙箱构建一个虚拟 Linux 根文件系统，解决编译器/解释器路径分散的问题。

## 问题背景

当前沙箱代码中硬编码了多个解释器路径：
- `/usr/bin/python3`
- `/usr/bin/node`
- `/usr/bin/pypy3`
- `java` (使用 execlp 查找)

同时，不同语言的编译器分散在系统的不同位置：
- gcc/g++: `/usr/bin/`
- go: `/usr/local/go/bin/`
- rustc: `~/.cargo/bin/`
- zig: `~/.zig/`

这导致：
1. 环境依赖性强，移植困难
2. 不同系统配置差异大
3. 编译时需要复杂的 PATH 设置

## 解决方案

构建一个虚拟根文件系统，将所有编译器/解释器统一到固定的相对路径下。

```
/opt/sandbox/
├── bin/          # 基本工具 (bash, sh, cat, ls 等)
├── usr/
│   └── bin/      # 所有编译器和解释器
│       ├── gcc
│       ├── g++
│       ├── javac
│       ├── java
│       ├── go
│       ├── rustc
│       ├── zig
│       ├── python3
│       ├── pypy3
│       ├── node
│       ├── mcs
│       └── mono
├── lib/          # 必要的系统库
├── lib64/
├── etc/          # 配置文件
└── home/
    └── workspace/ # 工作目录
```

## 使用方法

### 1. 构建虚拟系统

```bash
# 使用默认路径 /opt/sandbox
sudo ./scripts/setup_sandbox_root.sh

# 或指定自定义路径
sudo ./scripts/setup_sandbox_root.sh /path/to/sandbox
```

脚本会自动：
1. 创建目录结构
2. 查找系统中的编译器/解释器
3. 创建符号链接到 `usr/bin/`
4. 复制必要的库文件
5. 生成配置文件

### 2. 设置环境变量

```bash
# 在编译/运行沙箱程序前执行
source scripts/sandbox_env.sh
```

这会设置以下环境变量：
- `SANDBOX_ROOT`: 虚拟系统根目录
- `SANDBOX_PATH`: 虚拟系统的可执行文件路径
- `SANDBOX_LD_LIBRARY_PATH`: 虚拟系统的库路径

### 3. 在代码中使用

#### 方法 A: 修改 code_executor.cpp

在编译时设置 PATH 和库路径：

```cpp
// 在 compile() 函数中修改环境变量设置
const char* newPath = "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
                     ":/usr/local/go/bin:/snap/bin"
                     ":/home/ubuntu/.cargo/bin:/home/ubuntu/.zig";

// 改为使用沙箱路径
const char* sandboxRoot = getenv("SANDBOX_ROOT") ?: "/opt/sandbox";
std::string sandboxPath = std::string(sandboxRoot) + "/usr/bin";
std::string newPath = "PATH=" + sandboxPath + ":/bin:/usr/bin";
setenv("PATH", newPath.c_str(), 1);
```

#### 方法 B: 修改 languages.json

将所有编译命令改为使用绝对路径：

```json
{
  "cpp": {
    "compile_cmd": "/opt/sandbox/usr/bin/g++ -O2 -std=c++17 -o {output} {source}",
    "run_cmd": "{output}",
    "source_file": "main.cpp"
  },
  "python": {
    "compile_cmd": "",
    "run_cmd": "/opt/sandbox/usr/bin/python3 {source}",
    "source_file": "main.py"
  }
}
```

或者使用环境变量占位符：

```json
{
  "cpp": {
    "compile_cmd": "$SANDBOX/usr/bin/g++ -O2 -std=c++17 -o {output} {source}",
    "run_cmd": "{output}",
    "source_file": "main.cpp"
  }
}
```

#### 方法 C: 使用 chroot

在执行时使用 chroot 隔离到虚拟系统：

```cpp
// 在 executeInSandbox() 函数中
if (chdir(sandboxRoot.c_str()) == 0 && chroot(".") == 0) {
    // 现在已经进入虚拟系统
    // 执行程序时使用相对路径
    execl("/usr/bin/python3", "python3", sourceFile.c_str(), nullptr);
}
```

## 配置文件

脚本会生成 `config/sandbox_paths.json`：

```json
{
  "sandbox_root": "/opt/sandbox",
  "bin_path": "/opt/sandbox/usr/bin",
  "lib_path": "/opt/sandbox/usr/lib:/opt/sandbox/lib:/opt/sandbox/lib64",
  "compilers": {
    "gcc": "/opt/sandbox/usr/bin/gcc",
    "g++": "/opt/sandbox/usr/bin/g++",
    "javac": "/opt/sandbox/usr/bin/javac",
    "java": "/opt/sandbox/usr/bin/java",
    "go": "/opt/sandbox/usr/bin/go",
    "rustc": "/opt/sandbox/usr/bin/rustc",
    "zig": "/opt/sandbox/usr/bin/zig",
    "python3": "/opt/sandbox/usr/bin/python3",
    "node": "/opt/sandbox/usr/bin/node"
  }
}
```

## 目录结构

构建后的虚拟系统目录结构：

```
/opt/sandbox/
├── bin/              # 基本命令 (通过符号链接)
│   ├── bash
│   ├── sh
│   ├── cat
│   └── ls
├── usr/
│   └── bin/          # 编译器和解释器 (符号链接)
│       ├── gcc -> /usr/bin/gcc
│       ├── g++ -> /usr/bin/g++
│       ├── python3 -> /usr/bin/python3
│       └── ...
├── lib/              # 必要的共享库
│   └── x86_64-linux-gnu/
│       ├── libc.so.6
│       ├── libm.so.6
│       └── ...
├── lib64/
│   └── ld-linux-x86-64.so.2
├── etc/              # 配置文件
│   ├── passwd
│   ├── group
│   ├── hosts
│   └── resolv.conf
├── tmp/              # 临时文件 (权限 1777)
├── home/
│   └── workspace/    # 用户工作目录
└── proc/             # proc 文件系统 (挂载点)
```

## 优势

1. **统一管理**: 所有编译器/解释器在统一位置
2. **隔离性好**: 可以使用 chroot 进一步隔离
3. **可移植**: 只需要复制整个目录即可迁移
4. **版本固定**: 符号链接锁定特定版本
5. **易于维护**: 集中管理，更新方便

## 注意事项

1. **需要 root 权限**: 创建符号链接和某些操作需要 root
2. **库依赖**: 确保复制了所有必要的共享库
3. **磁盘空间**: 虚拟系统会占用一定磁盘空间
4. **权限问题**: 注意文件和目录的权限设置

## 故障排查

### 编译器找不到

```bash
# 检查符号链接
ls -la /opt/sandbox/usr/bin/gcc

# 检查目标是否存在
which gcc

# 重新运行脚本
sudo ./scripts/setup_sandbox_root.sh
```

### 库文件缺失

```bash
# 查看缺少的库
ldd /opt/sandbox/usr/bin/gcc

# 手动复制库
sudo cp /lib/x86_64-linux-gnu/libxxx.so /opt/sandbox/lib/x86_64-linux-gnu/
```

### 权限问题

```bash
# 修复权限
sudo chmod 1777 /opt/sandbox/tmp
sudo chmod -R 755 /opt/sandbox/usr/bin
```

## 扩展

### 添加新的语言支持

1. 确保系统中已安装对应编译器/解释器
2. 重新运行构建脚本
3. 在 `languages.json` 中添加配置

### 使用容器替代

如果需要更强的隔离，可以考虑使用 Docker 容器：

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc g++ make \
    python3 python3-pip \
    openjdk-17-jdk \
    golang-go \
    rustc cargo \
    nodejs npm

ENV SANDBOX_ROOT=/sandbox
WORKDIR $SANDBOX_ROOT
```

## 相关文件

- `scripts/setup_sandbox_root.sh`: 构建脚本
- `scripts/sandbox_env.sh`: 环境变量设置脚本
- `config/sandbox_paths.json`: 生成的路径配置
- `src/code_executor.cpp`: 需要修改的源代码文件
