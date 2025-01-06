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
git clone <repository-url>
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

## 运行服务

```bash
# 使用默认配置运行
./voice_assistant

# 自定义端口和模型路径
export SERVICE_PORT=8080
export MODEL_PATH=/path/to/model.onnx
./voice_assistant
```

## API 使用说明

### 1. 语音识别接口

**请求**：
```bash
POST /v1/speech:recognize
Content-Type: multipart/form-data
```

参数：
- `audio`: 音频文件（支持 WAV, MP3, FLAC, OGG）
- `config`: JSON 配置（可选）
  ```json
  {
    "encoding": "LINEAR16",
    "sample_rate_hertz": 16000,
    "language_code": "zh-CN",
    "enable_automatic_punctuation": true,
    "model": "default"
  }
  ```

**响应**：
```json
{
  "results": [{
    "alternatives": [{
      "transcript": "识别的文本",
      "confidence": 0.95
    }]
  }]
}
```

### 2. 健康检查接口

**请求**：
```bash
GET /health
```

**响应**：
```json
{
  "status": "healthy"
}
```

## 目录结构

```
.
├── CMakeLists.txt
├── README.md
├── include/
│   ├── api/
│   ├── core/
│   └── utils/
├── src/
│   ├── api/
│   ├── core/
│   └── utils/
├── dep/
│   └── SenseVoice.cpp/
└── models/
```

## 许可证

MIT License

## 贡献指南

1. Fork 项目
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request
