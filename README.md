# Voice Assistant

基于 sherpa-onnx 的语音识别 API 服务，提供与 Google Speech-to-Text API 兼容的接口。

## 功能特点

- 支持多种音频格式（WAV, MP3, FLAC, OGG）
- RESTful API 接口
- 支持多语言识别
- 高性能 C++ 实现
- 支持自动标点符号
- 支持实时语音识别和长时间语音识别
- 支持词时间戳标注
- 支持自定义词汇和语境优化

## 依赖项

- C++17 或更高版本
- CMake 3.10 或更高版本
- sherpa-onnx（作为子模块）
- cpp-httplib
- nlohmann/json
- libsndfile
- ONNX Runtime

## 构建说明

1. 克隆项目并初始化子模块：
```bash
git clone https://github.com/hewenyu/voice-assistant.git
cd voice-assistant
git submodule sync && git submodule update --init --recursive
```
