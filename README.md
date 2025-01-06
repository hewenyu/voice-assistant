# Voice Assistant

基于 SenseVoice 模型的语音识别 API 服务，提供与 Google Speech-to-Text API 兼容的接口。

## 功能特点

- 支持多种音频格式（WAV, MP3, FLAC, OGG）
- RESTful API 接口
- 支持多语言识别（中文、英文、日语、韩语、粤语）
- 高性能 C++ 实现
- 支持自动标点符号

## 依赖项

- C++17 或更高版本
- CMake 3.10 或更高版本
- SenseVoice.cpp（作为子模块）
- cpp-httplib
- nlohmann/json
- libsndfile

## 构建说明

1. 克隆项目并初始化子模块：
```bash
git clone https://github.com/hewenyu/voice-assistant.git
cd voice-assistant
git submodule sync && git submodule update --init --recursive
```

2. 安装依赖：
```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libsndfile1-dev nlohmann-json3-dev
```

3. 构建项目：
```bash
mkdir build && cd build
cmake ..
make
```
