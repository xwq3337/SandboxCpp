#!/bin/bash

# 快速启动脚本

set -e

echo "======================================"
echo "Code Runner - 快速启动脚本"
echo "======================================"
echo ""

# 检查是否已经构建
if [ ! -d "build" ]; then
    echo "📦 正在创建 build 目录..."
    mkdir build
fi

cd build

# 检查 CMake 配置
if [ ! -f "Makefile" ]; then
    echo "⚙️  正在配置 CMake..."
    cmake ..
fi

# 编译
echo "🔨 正在编译..."
make -j$(nproc)

echo ""
echo "✅ 编译完成！"
echo ""

# 检查是否有 root 权限
if [ "$EUID" -ne 0 ]; then 
    echo "⚠️  警告：需要 root 权限来运行服务器（用于 cgroups）"
    echo "请使用: sudo ./start.sh"
    exit 1
fi

# 启动服务器
echo "🚀 启动服务器..."
echo ""
./code_runner
