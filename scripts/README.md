# Scripts 目录说明

本目录包含沙箱构建脚本和一些预下载的编译器安装包。

## 文件说明

### 脚本文件

- `setup_sandbox_root.sh` - 沙箱虚拟系统构建脚本

### 预下载的安装包

⚠️ **重要**: 这些安装包是针对特定架构的。脚本会自动检测系统架构并下载匹配的版本。

当前目录中的文件：
- `go1.25.5.linux-amd64.tar.gz` - Go 1.25.5 for x86_64 (59.8 MB)
- `zig-x86_64-linux-0.16.0-dev.2187+e2338edb4.tar.xz` - Zig dev for x86_64 (55.4 MB)
- `node-v20.11.0-linux-x64.tar.xz` - Node.js 20.11.0 for x86_64 (示例，可选)

如果你的系统是 ARM64 (aarch64)，这些文件将不会被使用，脚本会自动下载 ARM64 版本。

## 使用方式

构建脚本会自动：

1. **检测系统架构** (x86_64 或 aarch64)
2. **检查本地文件** - 如果存在匹配架构的安装包则使用
3. **下载对应版本** - 如果本地没有则从网络下载

### 架构检测

脚本会自动检测以下架构：

| 系统架构 | uname -m 输出 | Go 架构 | Zig 架构 |
|---------|--------------|---------|----------|
| Intel/AMD 64位 | x86_64 | amd64 | x86_64 |
| ARM 64位 | aarch64 | arm64 | aarch64 |

### 运行脚本

```bash
# 脚本会自动检测架构并下载对应版本
sudo ./scripts/setup_sandbox_root.sh
```

## 本地文件使用

如果你想使用本地缓存以加快安装：

### x86_64 系统

```bash
# 下载对应版本
cd scripts
wget https://go.dev/dl/go1.23.6.linux-amd64.tar.gz
wget https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz
wget https://nodejs.org/dist/v20.11.0/node-v20.11.0-linux-x64.tar.xz
```

### ARM64 系统

```bash
# 下载对应版本
cd scripts
wget https://go.dev/dl/go1.23.6.linux-arm64.tar.gz
wget https://ziglang.org/download/0.13.0/zig-linux-aarch64-0.13.0.tar.xz
wget https://nodejs.org/dist/v20.11.0/node-v20.11.0-linux-arm64.tar.xz
```

## 文件命名规范

脚本使用以下命名规范查找本地文件：

### Go
- 格式: `go{VERSION}.linux-{ARCH}.tar.gz`
- x86_64 示例: `go1.23.6.linux-amd64.tar.gz`
- ARM64 示例: `go1.23.6.linux-arm64.tar.gz`

### Zig
- 格式: `zig-linux-{ARCH}-{VERSION}.tar.xz`
- x86_64 示例: `zig-linux-x86_64-0.13.0.tar.xz`
- ARM64 示例: `zig-linux-aarch64-0.13.0.tar.xz`

### Node.js
- 格式: `node-v{VERSION}-linux-{ARCH}.tar.xz`
- x86_64 示例: `node-v20.11.0-linux-x64.tar.xz`
- ARM64 示例: `node-v20.11.0-linux-arm64.tar.xz`

## 文件来源

### Go
- 官方下载页: https://go.dev/dl/
- 支持的架构: linux-amd64, linux-arm64
- 当前版本: 1.23.6

### Zig
- 官方下载页: https://ziglang.org/download/
- 支持的架构: x86_64, aarch64
- 当前版本: 0.13.0

### Node.js
- 官方下载页: https://nodejs.org/en/download
- 支持的架构: x64 (x86_64), arm64
- 当前版本: 20 LTS

## 故障排查

### 架构不匹配

如果看到 "cannot execute binary file: Exec format error" 错误：

```bash
# 检查系统架构
uname -m

# 检查二进制文件架构
file /opt/sandbox/usr/local/go/bin/go
file /opt/sandbox/usr/local/zig/zig

# 重新运行脚本（会自动下载正确架构）
sudo ./scripts/setup_sandbox_root.sh
```

### 清理错误安装

```bash
# 删除沙箱重新安装
sudo rm -rf /opt/sandbox
sudo ./scripts/setup_sandbox_root.sh
```

## 注意事项

1. **架构匹配**: 确保下载的安装包与系统架构匹配
2. **文件完整性**: 下载后建议验证文件的 SHA256 校验和
3. **版本一致性**: 本地文件版本应与脚本中定义的版本一致
4. **磁盘空间**: 这些文件占用约 115MB 磁盘空间

## 更新文件

更新到最新版本：

```bash
cd scripts

# 更新 Go (修改脚本中的 GO_VERSION)
wget https://go.dev/dl/go1.23.6.linux-amd64.tar.gz

# 更新 Zig (修改脚本中的 ZIG_BASE_NAME)
wget https://ziglang.org/download/0.13.0/zig-linux-x86_64-0.13.0.tar.xz

# 更新 Node.js (修改脚本中的 NODE_VERSION)
wget https://nodejs.org/dist/v20.11.0/node-v20.11.0-linux-x64.tar.xz
```

然后相应更新 `setup_sandbox_root.sh` 中的版本号。
