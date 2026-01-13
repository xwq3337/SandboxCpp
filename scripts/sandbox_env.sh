#!/bin/bash
# 沙箱环境变量设置脚本
# 使用方式: source scripts/sandbox_env.sh

export SANDBOX_ROOT="/opt/sandbox"
export SANDBOX_PATH="$SANDBOX_ROOT/usr/bin:$SANDBOX_ROOT/bin:$SANDBOX_ROOT/usr/local/bin:$SANDBOX_ROOT/usr/local/go/bin"
export SANDBOX_LD_LIBRARY_PATH="$SANDBOX_ROOT/usr/lib:$SANDBOX_ROOT/lib:$SANDBOX_ROOT/lib64:$SANDBOX_ROOT/usr/local/lib"

echo "沙箱环境变量已设置:"
echo "  SANDBOX_ROOT=$SANDBOX_ROOT"
echo "  SANDBOX_PATH=$SANDBOX_PATH"
echo "  SANDBOX_LD_LIBRARY_PATH=$SANDBOX_LD_LIBRARY_PATH"
echo ""
echo "测试编译器:"
echo "  $SANDBOX_ROOT/usr/bin/gcc --version"
echo "  $SANDBOX_ROOT/usr/bin/python3 --version"
echo "  $SANDBOX_ROOT/usr/bin/go version"
