#!/bin/bash

echo "测试按钮功能..."
echo "启动程序，请尝试点击按钮"
echo "如果按钮工作，你应该能看到 'Button clicked!' 的日志消息"
echo ""

cd build/bin
timeout 30s ./ZumaHD 2>&1 | grep -E "(Button clicked|Starting game|SceneMenu|Application)"