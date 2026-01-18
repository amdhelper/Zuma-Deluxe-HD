#!/bin/bash

# Zuma HD 启动脚本
echo "启动 Zuma HD..."

# 确保从正确的目录运行
SCRIPT_DIR="$(dirname "$0")"
cd "$SCRIPT_DIR/build/bin"

echo "当前目录: $(pwd)"
echo "检查资源文件..."

if [ ! -f "images/frog.png" ]; then
    echo "错误: 找不到 images/frog.png"
    echo "请确保从项目根目录运行此脚本"
    exit 1
fi

if [ ! -f "ZumaHD" ]; then
    echo "错误: 找不到 ZumaHD 可执行文件"
    echo "请先编译项目: cd build && make"
    exit 1
fi

echo "启动游戏..."
./ZumaHD