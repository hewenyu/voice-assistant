#!/bin/bash

# 检查依赖
command -v python3 >/dev/null 2>&1 || { echo "需要 python3 但未安装。正在退出。"; exit 1; }
command -v pip3 >/dev/null 2>&1 || { echo "需要 pip3 但未安装。正在退出。"; exit 1; }

# 安装 Python 依赖
pip3 install -r requirements.txt

# 创建测试数据目录（如果不存在）
mkdir -p test_data

# 检查测试音频文件是否存在
if [ ! -f "test_data/zh.wav" ] || \
   [ ! -f "test_data/en.wav" ] || \
   [ ! -f "test_data/ja.wav" ] || \
   [ ! -f "test_data/ko.wav" ] || \
   [ ! -f "test_data/yue.wav" ]; then
    echo "警告：缺少测试音频文件。请确保所有必要的测试音频文件都在 test_data 目录中。"
    exit 1
fi

# 运行测试
python3 -m pytest test_api.py -v 