#!/bin/bash

##############################################################################
# SandBox 虚拟系统构建脚本 (网络安装版)
#
# 功能：为沙箱构建一个独立的虚拟根文件系统，从网络下载所有编译器
# 用法：sudo ./setup_sandbox_root.sh [根目录路径]
#
# 示例：sudo ./setup_sandbox_root.sh /opt/sandbox
#
# 支持的镜像源：
#   - Rust: https://rsproxy.cn
#   - Go: https://goproxy.cn
##############################################################################

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# 检查是否以 root 权限运行
check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "此脚本需要 root 权限运行，请使用 sudo"
        exit 1
    fi
}

# 获取当前用户名（非 root）
get_current_user() {
    if [ -n "$SUDO_USER" ]; then
        echo "$SUDO_USER"
    elif [ -n "$USER" ] && [ "$USER" != "root" ]; then
        echo "$USER"
    else
        # 尝试从登录用户获取
        logname 2>/dev/null || echo "nobody"
    fi
}

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 默认根目录
SANDBOX_ROOT="${1:-/opt/sandbox}"
SANDBOX_TMP="$SANDBOX_ROOT/tmp_install"

# 当前用户
CURRENT_USER=$(get_current_user)

log_info "========================================="
log_info "沙箱虚拟系统构建脚本 (网络安装版)"
log_info "========================================="
log_info "目标根目录: $SANDBOX_ROOT"
log_info "当前用户: $CURRENT_USER"

# 检查是否已存在
if [ -d "$SANDBOX_ROOT" ]; then
    log_warn "目录 $SANDBOX_ROOT 已存在"
    read -p "是否删除并重新创建? (y/N): " confirm
    if [[ $confirm =~ ^[Yy]$ ]]; then
        log_info "删除现有目录..."
        rm -rf "$SANDBOX_ROOT"
    else
        log_error "取消操作"
        exit 1
    fi
fi

##############################################################################
# 0. 准备工作
##############################################################################
log_step "准备工作..."

# 检测系统架构
ARCH=$(uname -m)
case "$ARCH" in
    x86_64|i686|i386)
        GO_ARCH="amd64"
        ZIG_ARCH="x86_64"
        ;;
    aarch64|arm64)
        GO_ARCH="arm64"
        ZIG_ARCH="aarch64"
        ;;
    *)
        log_error "不支持的系统架构: $ARCH"
        exit 1
        ;;
esac
log_info "检测到系统架构: $ARCH (Go: $GO_ARCH, Zig: $ZIG_ARCH)"

# 清理可能存在的有问题的 Mono 仓库配置
if [ -f /etc/apt/sources.list.d/mono-official-stable.list ]; then
    log_info "清理旧的 Mono 仓库配置..."
    rm -f /etc/apt/sources.list.d/mono-official-stable.list
fi

# 更新 apt 包列表
log_info "更新 apt 包列表..."
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq 2>&1 | grep -v "NO_PUBKEY" || true

# 安装必要工具
log_info "安装必要工具..."
apt-get install -y -qq \
    curl \
    wget \
    ca-certificates \
    gnupg \
    lsb-release \
    software-properties-common \
    build-essential \
    pkg-config \
    libssl-dev \
    > /dev/null

# 创建临时目录
mkdir -p "$SANDBOX_TMP"

##############################################################################
# 1. 创建基本目录结构
##############################################################################
log_step "创建目录结构..."

mkdir -p "$SANDBOX_ROOT"/{bin,boot,etc,lib,lib64,opt,proc,root,run,sbin,srv,sys,tmp,var}
mkdir -p "$SANDBOX_ROOT"/usr/{bin,sbin,lib,lib64,local/{bin,lib,lib64}}
mkdir -p "$SANDBOX_ROOT"/var/{log,tmp,run}
mkdir -p "$SANDBOX_ROOT"/home/workspace

# 设置权限
chmod 1777 "$SANDBOX_ROOT/tmp"
chmod 755 "$SANDBOX_ROOT"/{bin,boot,etc,lib,lib64,opt,root,run,sbin,srv,sys,var}
chmod 755 "$SANDBOX_ROOT"/usr/{bin,sbin,lib,lib64,local/{bin,lib,lib64}}

##############################################################################
# 2. 创建必要的配置文件
##############################################################################
log_step "创建配置文件..."

# /etc/passwd
cat > "$SANDBOX_ROOT/etc/passwd" << EOF
root:x:0:0:root:/root:/bin/bash
nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
$CURRENT_USER:x:1000:1000:Sandbox User:/home/workspace:/bin/bash
EOF

# /etc/group
cat > "$SANDBOX_ROOT/etc/group" << EOF
root:x:0:
nogroup:x:65534:
$CURRENT_USER:x:1000:
EOF

# /etc/hostname
cat > "$SANDBOX_ROOT/etc/hostname" << 'EOF'
sandbox
EOF

# /etc/resolv.conf (用于 DNS 解析)
cat > "$SANDBOX_ROOT/etc/resolv.conf" << 'EOF'
nameserver 8.8.8.8
nameserver 8.8.4.4
EOF

# /etc/hosts
cat > "$SANDBOX_ROOT/etc/hosts" << 'EOF'
127.0.0.1   localhost localhost.localdomain
::1         localhost localhost.localdomain
EOF

##############################################################################
# 3. 安装 GCC/G++ (C/C++)
##############################################################################
log_step "安装 GCC/G++..."

log_info "从 apt 安装 gcc/g++..."
apt-get install -y -qq gcc g++ > /dev/null

# 复制到沙箱
log_info "复制 gcc/g++ 到沙箱..."
mkdir -p "$SANDBOX_ROOT/usr/bin"
# 复制 gcc 相关文件（包括符号链接目标）
for file in /usr/bin/gcc* /usr/bin/g++* /usr/bin/cc /usr/bin/c++; do
    if [ -e "$file" ]; then
        cp -a "$file" "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
    fi
done
# 复制实际的编译器二进制文件
for file in /usr/bin/gcc-* /usr/bin/g++-*; do
    if [ -e "$file" ]; then
        cp -a "$file" "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
    fi
done
# 复制架构特定的 gcc/g++ 二进制文件（如 aarch64-linux-gnu-gcc-11）
if ls /usr/bin/aarch64-linux-gnu-gcc* /usr/bin/aarch64-linux-gnu-g++* >/dev/null 2>&1; then
    cp -a /usr/bin/aarch64-linux-gnu-gcc* /usr/bin/aarch64-linux-gnu-g++* "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
fi
if ls /usr/bin/x86_64-linux-gnu-gcc* /usr/bin/x86_64-linux-gnu-g++* >/dev/null 2>&1; then
    cp -a /usr/bin/x86_64-linux-gnu-gcc* /usr/bin/x86_64-linux-gnu-g++* "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
fi

# 复制 gcc 相关库
log_info "复制 gcc 库文件..."
for dir in /usr/lib/gcc /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /lib/aarch64-linux-gnu; do
    if [ -d "$dir" ]; then
        mkdir -p "$SANDBOX_ROOT$dir"
        cp -rn "$dir"/* "$SANDBOX_ROOT$dir/" 2>/dev/null || true
    fi
done

# 复制 C++ 标准库头文件
log_info "复制 C++ 标准库头文件..."
if [ -d "/usr/include/c++" ]; then
    mkdir -p "$SANDBOX_ROOT/usr/include"
    cp -rn /usr/include/c++ "$SANDBOX_ROOT/usr/include/" 2>/dev/null || true
    log_info "  已复制 /usr/include/c++"
fi

# 复制架构特定的 C++ 头文件（如 aarch64-linux-gnu/c++/11/bits/c++config.h）
log_info "复制架构特定的 C++ 头文件..."
for arch_dir in /usr/include/aarch64-linux-gnu /usr/include/x86_64-linux-gnu /usr/include/i386-linux-gnu; do
    if [ -d "$arch_dir" ]; then
        mkdir -p "$SANDBOX_ROOT/usr/include"
        cp -rn "$arch_dir" "$SANDBOX_ROOT/usr/include/" 2>/dev/null || true
        log_info "  已复制 $(basename $arch_dir)"
    fi
done

log_info "GCC 版本:"
"$SANDBOX_ROOT/usr/bin/gcc" --version 2>/dev/null | head -1 || echo "  安装失败"

##############################################################################
# 4. 安装 Python3
##############################################################################
log_step "安装 Python3..."

log_info "从 apt 安装 python3..."
apt-get install -y -qq python3 python3-pip python3-dev > /dev/null

# 复制到沙箱
log_info "复制 python3 到沙箱..."
cp -a /usr/bin/python3* "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true

# 复制 Python 库
log_info "复制 Python 库文件..."
for dir in /usr/lib/python3 /usr/local/lib/python3; do
    if [ -d "$dir" ]; then
        # 只复制必要的库，减小体积
        mkdir -p "$SANDBOX_ROOT$(dirname "$dir")"
        # 创建目录结构
        find "$dir" -maxdepth 1 -type d | while read subdir; do
            mkdir -p "$SANDBOX_ROOT$subdir"
        done
        # 复制标准库
        cp -rn "$dir"/collections "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -rn "$dir"/enum "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -rn "$dir"/re "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -rn "$dir"/json "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -rn "$dir"/typing "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -rn "$dir"/math "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -rn "$dir"/sysconfig "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -a "$dir"/*.so "$SANDBOX_ROOT$dir/" 2>/dev/null || true
        cp -a "$dir"/_sysconfigdata*.py "$SANDBOX_ROOT$dir/" 2>/dev/null || true
    fi
done

log_info "Python 版本:"
"$SANDBOX_ROOT/usr/bin/python3" --version 2>/dev/null || echo "  安装失败"

##############################################################################
# 5. 安装 Java (OpenJDK)
##############################################################################
log_step "安装 Java..."

log_info "从 apt 安装 OpenJDK..."
apt-get install -y -qq default-jdk > /dev/null

# 复制到沙箱
log_info "复制 java/javac 到沙箱..."
cp -a /usr/bin/java* "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
cp -a /usr/bin/jmap "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true

# 复制 Java 库
log_info "复制 Java 库文件..."
JAVA_HOME=$(dirname $(dirname $(readlink -f $(which java))))
if [ -d "$JAVA_HOME" ]; then
    mkdir -p "$SANDBOX_ROOT$JAVA_HOME"
    cp -rn "$JAVA_HOME"/* "$SANDBOX_ROOT$JAVA_HOME/" 2>/dev/null || true
fi

log_info "Java 版本:"
"$SANDBOX_ROOT/usr/bin/java" -version 2>&1 | head -1 || echo "  安装失败"

##############################################################################
# 6. 安装 Go
##############################################################################
log_step "安装 Go..."

GO_VERSION="1.23.6"
GO_FILE="go${GO_VERSION}.linux-${GO_ARCH}.tar.gz"
GO_INSTALL_PATH="$SANDBOX_ROOT/usr/local/go"

# 优先使用本地 Go 安装包（查找任何匹配架构的 Go 包）
LOCAL_GO_FILE=$(ls "$SCRIPT_DIR"/go*.linux-${GO_ARCH}.tar.gz 2>/dev/null | head -1)
if [ -n "$LOCAL_GO_FILE" ]; then
    log_info "使用本地 Go 安装包: $LOCAL_GO_FILE"
    cp "$LOCAL_GO_FILE" "$SANDBOX_TMP/$GO_FILE"
    GO_INSTALLED=true
    # 从文件名提取实际版本
    ACTUAL_GO_VERSION=$(basename "$LOCAL_GO_FILE" | sed -E 's/go([0-9.]+)\.linux-.*/\1/')
    log_info "本地 Go 版本: $ACTUAL_GO_VERSION"
else
    log_info "从官方源下载 Go ${GO_VERSION} (linux-${GO_ARCH})..."
    cd "$SANDBOX_TMP"
    wget -q --show-progress "https://go.dev/dl/${GO_FILE}" -O "$GO_FILE" || {
        log_error "Go 下载失败"
        GO_INSTALLED=false
    }
fi

if [ "$GO_INSTALLED" != "false" ]; then
    cd "$SANDBOX_TMP"
    log_info "解压 Go 到 $GO_INSTALL_PATH..."
    mkdir -p "$GO_INSTALL_PATH"
    # 先解压到临时目录，然后移动
    tar -xzf "$GO_FILE" -C "$SANDBOX_TMP/"
    mv go/* "$GO_INSTALL_PATH/"
    rmdir go 2>/dev/null || true

    # 创建符号链接
    ln -sf "$GO_INSTALL_PATH/bin/go" "$SANDBOX_ROOT/usr/bin/go"
    ln -sf "$GO_INSTALL_PATH/bin/gofmt" "$SANDBOX_ROOT/usr/bin/gofmt"

    # 配置 Go 使用国内镜像
    log_info "配置 Go 国内镜像..."
    export GOPROXY=https://goproxy.cn,direct
    export GOSUMDB=goproxy.cn/sumdb/sum.golang.org

    # 测试 Go 是否可用
    if "$SANDBOX_ROOT/usr/bin/go" version >/dev/null 2>&1; then
        log_info "Go 版本:"
        "$SANDBOX_ROOT/usr/bin/go" version
    else
        log_warn "Go 安装完成但测试失败，可能缺少库依赖"
    fi
fi

##############################################################################
# 7. 安装 Rust
##############################################################################
log_step "安装 Rust..."

# 设置 Rust 安装路径到沙箱（必须先设置环境变量）
export RUSTUP_HOME="$SANDBOX_ROOT/usr/local/rustup"
export CARGO_HOME="$SANDBOX_ROOT/usr/local/cargo"

log_info "下载并安装 Rust（使用国内镜像）..."

# 设置 Rust 国内镜像
export RUSTUP_DIST_SERVER="https://rsproxy.cn"
export RUSTUP_UPDATE_ROOT="https://rsproxy.cn/rustup"

# 下载并执行 rustup 安装脚本（会直接安装到 CARGO_HOME）
cd "$SANDBOX_TMP"
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- \
    -y \
    --no-modify-path \
    --default-toolchain stable \
    --profile minimal \
    > /dev/null 2>&1 || {
    log_warn "Rust 国内镜像安装失败，尝试官方源..."
    unset RUSTUP_DIST_SERVER
    unset RUSTUP_UPDATE_ROOT
    curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- \
        -y \
        --no-modify-path \
        --default-toolchain stable \
        --profile minimal \
        > /dev/null 2>&1 || {
        log_error "Rust 安装失败"
        RUST_INSTALLED=false
    }
}

if [ "$RUST_INSTALLED" != "false" ]; then
    # 创建符号链接
    ln -sf "$CARGO_HOME/bin/rustc" "$SANDBOX_ROOT/usr/bin/rustc"
    ln -sf "$CARGO_HOME/bin/cargo" "$SANDBOX_ROOT/usr/bin/cargo"
    ln -sf "$CARGO_HOME/bin/rustup" "$SANDBOX_ROOT/usr/bin/rustup"

    # 测试 Rust 是否可用
    if "$SANDBOX_ROOT/usr/bin/rustc" --version >/dev/null 2>&1; then
        log_info "Rust 版本:"
        "$SANDBOX_ROOT/usr/bin/rustc" --version
    else
        log_warn "Rust 安装完成但测试失败，可能缺少库依赖"
    fi
fi

##############################################################################
# 8. 安装 Node.js
##############################################################################
log_step "安装 Node.js..."

# 优先使用本地 Node.js 安装包（查找任何匹配架构的 Node.js 包）
LOCAL_NODE_FILE=$(ls "$SCRIPT_DIR"/node*${GO_ARCH}*.tar.xz 2>/dev/null | head -1)
NODE_INSTALLED=false

if [ -n "$LOCAL_NODE_FILE" ]; then
    log_info "使用本地 Node.js 安装包: $LOCAL_NODE_FILE"

    cd "$SANDBOX_TMP"
    cp "$LOCAL_NODE_FILE" .

    # 解压 Node.js
    log_info "解压 Node.js..."
    tar -xf "$(basename "$LOCAL_NODE_FILE")" || {
        log_error "Node.js 解压失败"
        NODE_INSTALLED=false
    }

    # 自动检测解压后的目录名
    NODE_EXTRACTED_DIR=$(ls -d node-* 2>/dev/null | head -1)
    if [ -z "$NODE_EXTRACTED_DIR" ]; then
        log_error "找不到 Node.js 解压目录"
        NODE_INSTALLED=false
    else
        log_info "检测到 Node.js 目录: $NODE_EXTRACTED_DIR"

        # 复制到沙箱
        mkdir -p "$SANDBOX_ROOT/usr/local/node"
        cp -a "$NODE_EXTRACTED_DIR"/* "$SANDBOX_ROOT/usr/local/node/"

        # 创建符号链接到 bin 目录
        ln -sf "$SANDBOX_ROOT/usr/local/node/bin/node" "$SANDBOX_ROOT/usr/bin/node"
        ln -sf "$SANDBOX_ROOT/usr/local/node/bin/npm" "$SANDBOX_ROOT/usr/bin/npm"
        ln -sf "$SANDBOX_ROOT/usr/local/node/bin/npx" "$SANDBOX_ROOT/usr/bin/npx"
        ln -sf "$SANDBOX_ROOT/usr/local/node/bin/corepack" "$SANDBOX_ROOT/usr/bin/corepack"

        NODE_INSTALLED=true
    fi
else
    log_info "从 NodeSource 仓库安装 Node.js..."
    NODE_VERSION="20"

    # 添加 NodeSource 仓库
    curl -fsSL https://deb.nodesource.com/setup_${NODE_VERSION}.x | bash - > /dev/null 2>&1 || {
        log_warn "NodeSource 仓库添加失败，尝试 apt 安装..."
        apt-get install -y -qq nodejs npm > /dev/null 2>&1
        NODE_INSTALLED=true
    }

    # 如果添加仓库成功，则安装 nodejs
    if [ "$NODE_INSTALLED" != "true" ]; then
        apt-get install -y -qq nodejs > /dev/null 2>&1 || {
            log_error "Node.js 安装失败"
            NODE_INSTALLED=false
        }
    fi

    # 检查 node 是否安装成功
    if command -v node >/dev/null 2>&1; then
        NODE_INSTALLED=true
    fi

    if [ "$NODE_INSTALLED" != "false" ]; then
        # 复制到沙箱
        log_info "复制 node/npm 到沙箱..."
        cp -a /usr/bin/node "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
        cp -a /usr/bin/npm "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
        cp -a /usr/bin/npx "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true

        # 复制 Node.js 库
        NODE_PATH=$(dirname $(dirname $(readlink -f $(which node))))
        if [ -d "$NODE_PATH/lib/node_modules" ]; then
            mkdir -p "$SANDBOX_ROOT$NODE_PATH/lib"
            cp -rn "$NODE_PATH/lib/node_modules" "$SANDBOX_ROOT$NODE_PATH/lib/" 2>/dev/null || true
        fi
    fi
fi

if [ "$NODE_INSTALLED" != "false" ]; then
    # 测试 Node 是否可用
    if "$SANDBOX_ROOT/usr/bin/node" --version >/dev/null 2>&1; then
        log_info "Node.js 版本:"
        "$SANDBOX_ROOT/usr/bin/node" --version
    else
        log_warn "Node.js 安装完成但测试失败，可能缺少库依赖"
    fi
fi

##############################################################################
# 9. 安装 Zig
##############################################################################
log_step "安装 Zig..."

# 优先使用本地 Zig 安装包（查找任何匹配架构的 Zig 包）
LOCAL_ZIG_FILE=$(ls "$SCRIPT_DIR"/zig*${ZIG_ARCH}*.tar.xz 2>/dev/null | head -1)
if [ -n "$LOCAL_ZIG_FILE" ]; then
    log_info "使用本地 Zig 安装包: $LOCAL_ZIG_FILE"
    ZIG_LOCAL_FILE=$(basename "$LOCAL_ZIG_FILE")
    cp "$LOCAL_ZIG_FILE" "$SANDBOX_TMP/$ZIG_LOCAL_FILE"
    ZIG_INSTALLED=true
else
    # Zig 的本地文件名（从 scripts 目录）
    # 支持动态架构检测
    ZIG_BASE_NAME="zig-linux-${ZIG_ARCH}-0.16.0"
    ZIG_LOCAL_FILE="${ZIG_BASE_NAME}.tar.xz"

    log_info "本地未找到 Zig 安装包，尝试下载 (${ZIG_ARCH})..."
    cd "$SANDBOX_TMP"
    wget -q --show-progress "https://ziglang.org/download/0.13.0/${ZIG_LOCAL_FILE}" -O "$ZIG_LOCAL_FILE" || {
        log_warn "Zig 下载失败"
        ZIG_INSTALLED=false
    }
fi

if [ "$ZIG_INSTALLED" != "false" ]; then
    cd "$SANDBOX_TMP"
    log_info "解压 Zig..."
    tar -xf "$ZIG_LOCAL_FILE"

    # 自动检测解压后的目录名（因为不同版本的命名可能不同）
    ZIG_EXTRACTED_DIR=$(ls -d zig-* 2>/dev/null | head -1)
    if [ -z "$ZIG_EXTRACTED_DIR" ]; then
        log_error "找不到 Zig 解压目录"
        ZIG_INSTALLED=false
    else
        log_info "检测到 Zig 目录: $ZIG_EXTRACTED_DIR"
        mkdir -p "$SANDBOX_ROOT/usr/local/zig"
        # 使用 cp -a 保留所有属性，包括权限
        cp -a "$ZIG_EXTRACTED_DIR"/* "$SANDBOX_ROOT/usr/local/zig/"

        # 创建符号链接
        ln -sf "$SANDBOX_ROOT/usr/local/zig/zig" "$SANDBOX_ROOT/usr/bin/zig"

        # 测试 Zig 是否可用
        if "$SANDBOX_ROOT/usr/bin/zig" version >/dev/null 2>&1; then
            log_info "Zig 版本:"
            "$SANDBOX_ROOT/usr/bin/zig" version
        else
            log_warn "Zig 安装完成但测试失败，可能缺少库依赖"
        fi
    fi
fi

##############################################################################
# 10. 安装 C# (Mono)
##############################################################################
log_step "安装 Mono (C#)..."

# 检查系统是否已有 mono
if command -v mono >/dev/null 2>&1; then
    log_info "系统已安装 Mono，直接复制到沙箱..."
    MONO_INSTALLED=true
else
    # 先尝试从 apt 直接安装（如果系统源中有）
    log_info "尝试从系统源安装 Mono..."
    apt-get install -y -qq mono-runtime mono-devel mono-utils > /dev/null 2>&1 && {
        MONO_INSTALLED=true
    }

    # 如果系统源没有，再添加 Mono 官方仓库
    if [ "$MONO_INSTALLED" != "true" ]; then
        log_info "添加 Mono 官方仓库..."
        apt-get install -y -qq gnupg ca-certificates > /dev/null 2>&1 || true

        # 下载 GPG 密钥
        curl -fsSL https://download.mono-project.com/repo/stable.deb/sigs-key.asc 2>/dev/null | gpg --dearmor > /usr/share/keyrings/mono-official-stable.gpg 2>/dev/null || true

        # 添加仓库（允许不签名）
        if [ -f /usr/share/keyrings/mono-official-stable.gpg ]; then
            echo "deb [signed-by=/usr/share/keyrings/mono-official-stable.gpg trusted=yes] https://download.mono-project.com/repo/debian stable-buster main" > /etc/apt/sources.list.d/mono-official-stable.list 2>/dev/null || true
        else
            # 如果 GPG 密钥下载失败，使用不验证的方式
            echo "deb [trusted=yes] https://download.mono-project.com/repo/debian stable-buster main" > /etc/apt/sources.list.d/mono-official-stable.list 2>/dev/null || true
        fi

        apt-get update -qq > /dev/null 2>&1 || true

        log_info "从 Mono 官方仓库安装..."
        apt-get install -y -qq mono-complete > /dev/null 2>&1 || {
            log_warn "Mono 完整版安装失败，尝试基础版..."
            apt-get install -y -qq mono-runtime mono-devel > /dev/null 2>&1 || {
                log_error "Mono 安装失败"
                MONO_INSTALLED=false
            }
        }
    fi

    # 检查 mono 是否安装成功
    if command -v mono >/dev/null 2>&1; then
        MONO_INSTALLED=true
    fi
fi

if [ "$MONO_INSTALLED" != "false" ]; then
    # 复制到沙箱
    log_info "复制 mono/mcs 到沙箱..."
    cp -a /usr/bin/mono* "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
    cp -a /usr/bin/mcs "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true

    # 复制 Mono 库
    if [ -d /usr/lib/mono ]; then
        mkdir -p "$SANDBOX_ROOT/usr/lib"
        cp -rn /usr/lib/mono "$SANDBOX_ROOT/usr/lib/" 2>/dev/null || true
    fi

    log_info "Mono 版本:"
    "$SANDBOX_ROOT/usr/bin/mono" --version 2>/dev/null | head -1 || echo "  安装失败"
fi

##############################################################################
# 11. 复制基本的系统库和工具
##############################################################################
log_step "复制系统库和基本工具..."

# 基本工具
TOOLS="bash sh cat ls echo rm cp mv mkdir rmdir head tail grep sed awk cut tr wc sort uniq find xargs"
for tool in $TOOLS; do
    if [ -e "/bin/$tool" ]; then
        cp -a "/bin/$tool" "$SANDBOX_ROOT/bin/" 2>/dev/null || true
    elif [ -e "/usr/bin/$tool" ]; then
        cp -a "/usr/bin/$tool" "$SANDBOX_ROOT/usr/bin/" 2>/dev/null || true
    fi
done

# 复制基本系统库
log_info "复制基本系统库..."
for lib in /lib/x86_64-linux-gnu/libc.so.6 \
           /lib/x86_64-linux-gnu/libm.so.6 \
           /lib/x86_64-linux-gnu/libpthread.so.0 \
           /lib/x86_64-linux-gnu/libdl.so.2 \
           /lib/x86_64-linux-gnu/librt.so.1 \
           /lib/x86_64-linux-gnu/libutil.so.1 \
           /lib/x86_64-linux-gnu/libnsl.so.1 \
           /lib/x86_64-linux-gnu/libresolv.so.2 \
           /lib64/ld-linux-x86-64.so.2; do
    if [ -f "$lib" ] && [ ! -e "$SANDBOX_ROOT$lib" ]; then
        mkdir -p "$SANDBOX_ROOT$(dirname "$lib")"
        cp -L "$lib" "$SANDBOX_ROOT$lib" 2>/dev/null || true
    fi
done

# 复制 ldconfig 缓存
if [ -f /etc/ld.so.cache ]; then
    cp /etc/ld.so.cache "$SANDBOX_ROOT/etc/" 2>/dev/null || true
fi

##############################################################################
# 12. 清理临时文件
##############################################################################
log_step "清理临时文件..."

log_info "删除 $SANDBOX_TMP..."
rm -rf "$SANDBOX_TMP"

##############################################################################
# 13. 创建配置文件
##############################################################################
log_step "生成配置文件..."

# 收集版本信息
log_info "收集编译器版本信息..."

GCC_VERSION=$("$SANDBOX_ROOT/usr/bin/gcc" --version 2>/dev/null | head -1 || echo "未安装")
PYTHON_VERSION=$("$SANDBOX_ROOT/usr/bin/python3" --version 2>&1 || echo "未安装")
JAVA_VERSION=$("$SANDBOX_ROOT/usr/bin/java" -version 2>&1 | head -1 || echo "未安装")
GO_VERSION=$("$SANDBOX_ROOT/usr/bin/go" version 2>&1 || echo "未安装")
RUST_VERSION=$("$SANDBOX_ROOT/usr/bin/rustc" --version 2>&1 || echo "未安装")
NODE_VERSION=$("$SANDBOX_ROOT/usr/bin/node" --version 2>&1 || echo "未安装")
ZIG_VERSION=$("$SANDBOX_ROOT/usr/bin/zig" version 2>&1 || echo "未安装")
MONO_VERSION=$("$SANDBOX_ROOT/usr/bin/mono" --version 2>/dev/null | head -1 || echo "未安装")

# 生成 JSON 文件（包含版本信息）
cat > "$PROJECT_ROOT/config/sandbox_paths.json" << EOF
{
  "sandbox_root": "$SANDBOX_ROOT",
  "bin_path": "$SANDBOX_ROOT/usr/bin",
  "lib_path": "$SANDBOX_ROOT/usr/lib:$SANDBOX_ROOT/lib:$SANDBOX_ROOT/lib64",
  "compilers": {
    "gcc": "$SANDBOX_ROOT/usr/bin/gcc",
    "g++": "$SANDBOX_ROOT/usr/bin/g++",
    "javac": "$SANDBOX_ROOT/usr/bin/javac",
    "java": "$SANDBOX_ROOT/usr/bin/java",
    "go": "$SANDBOX_ROOT/usr/bin/go",
    "rustc": "$SANDBOX_ROOT/usr/bin/rustc",
    "zig": "$SANDBOX_ROOT/usr/bin/zig",
    "mcs": "$SANDBOX_ROOT/usr/bin/mcs",
    "mono": "$SANDBOX_ROOT/usr/bin/mono",
    "python3": "$SANDBOX_ROOT/usr/bin/python3",
    "node": "$SANDBOX_ROOT/usr/bin/node",
    "npm": "$SANDBOX_ROOT/usr/bin/npm"
  },
  "versions": {
    "generated_at": "$(date -Iseconds)",
    "gcc": $(echo "$GCC_VERSION" | jq -Rs .),
    "python": $(echo "$PYTHON_VERSION" | jq -Rs .),
    "java": $(echo "$JAVA_VERSION" | jq -Rs .),
    "go": $(echo "$GO_VERSION" | jq -Rs .),
    "rust": $(echo "$RUST_VERSION" | jq -Rs .),
    "node": $(echo "$NODE_VERSION" | jq -Rs .),
    "zig": $(echo "$ZIG_VERSION" | jq -Rs .),
    "mono": $(echo "$MONO_VERSION" | jq -Rs .)
  }
}
EOF

log_info "配置文件已保存到: $PROJECT_ROOT/config/sandbox_paths.json"


##############################################################################
# 14. 创建环境变量设置脚本
##############################################################################
log_step "生成环境设置脚本..."

cat > "$PROJECT_ROOT/scripts/sandbox_env.sh" << EOF
#!/bin/bash
# 沙箱环境变量设置脚本
# 使用方式: source scripts/sandbox_env.sh

export SANDBOX_ROOT="$SANDBOX_ROOT"
export SANDBOX_PATH="\$SANDBOX_ROOT/usr/bin:\$SANDBOX_ROOT/bin:\$SANDBOX_ROOT/usr/local/bin:\$SANDBOX_ROOT/usr/local/go/bin"
export SANDBOX_LD_LIBRARY_PATH="\$SANDBOX_ROOT/usr/lib:\$SANDBOX_ROOT/lib:\$SANDBOX_ROOT/lib64:\$SANDBOX_ROOT/usr/local/lib"

echo "沙箱环境变量已设置:"
echo "  SANDBOX_ROOT=\$SANDBOX_ROOT"
echo "  SANDBOX_PATH=\$SANDBOX_PATH"
echo "  SANDBOX_LD_LIBRARY_PATH=\$SANDBOX_LD_LIBRARY_PATH"
echo ""
echo "测试编译器:"
echo "  \$SANDBOX_ROOT/usr/bin/gcc --version"
echo "  \$SANDBOX_ROOT/usr/bin/python3 --version"
echo "  \$SANDBOX_ROOT/usr/bin/go version"
EOF

chmod +x "$PROJECT_ROOT/scripts/sandbox_env.sh"

##############################################################################
# 15. 完成
##############################################################################
echo ""
log_info "========================================="
log_info "沙箱虚拟系统构建完成!"
log_info "========================================="
log_info ""
log_info "根目录: $SANDBOX_ROOT"
log_info "占用空间: $(du -sh $SANDBOX_ROOT | cut -f1)"
log_info ""
log_info "已安装的编译器/解释器:"
echo "  C/C++:    $SANDBOX_ROOT/usr/bin/gcc"
echo "  Python:   $SANDBOX_ROOT/usr/bin/python3"
echo "  Java:     $SANDBOX_ROOT/usr/bin/java"
echo "  Go:       $SANDBOX_ROOT/usr/bin/go"
echo "  Rust:     $SANDBOX_ROOT/usr/bin/rustc"
echo "  Node.js:  $SANDBOX_ROOT/usr/bin/node"
echo "  Zig:      $SANDBOX_ROOT/usr/bin/zig"
echo "  C#:       $SANDBOX_ROOT/usr/bin/mcs"
log_info ""
log_info "下一步操作:"
log_info "  1. 源环境变量: source $PROJECT_ROOT/scripts/sandbox_env.sh"
log_info "  2. 查看配置: cat $PROJECT_ROOT/config/sandbox_paths.json"
log_info "  3. 在代码中使用 SANDBOX_ROOT 环境变量"
log_info ""
log_info "示例编译命令:"
echo "  \$SANDBOX_ROOT/usr/bin/gcc -o output source.c"
echo "  \$SANDBOX_ROOT/usr/bin/python3 script.py"
echo "  \$SANDBOX_ROOT/usr/bin/go run main.go"
log_info ""
