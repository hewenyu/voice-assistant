# Voice Assistant

基于 SenseVoice 模型的语音识别 API 服务，提供与 Google Speech-to-Text API 兼容的接口。

## 功能特点

- 支持多种音频格式（WAV, MP3, FLAC, OGG）
- RESTful API 接口
- 支持多语言识别（中文、英文、日语、韩语、粤语）
- 高性能 C++ 实现
- 支持自动标点符号
- 支持实时语音识别和长时间语音识别
- 支持词时间戳标注
- 支持自定义词汇和语境优化

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

## API 接口说明

### 1. 健康检查接口

```http
GET /health
```

响应示例：
```json
{
    "status": "OK",
    "message": "Service is healthy"
}
```

### 2. 同步语音识别

```http
POST /recognize
Content-Type: application/json
```

请求体示例：
```json
{
    "config": {
        "encoding": "LINEAR16",
        "sampleRateHertz": 16000,
        "languageCode": "zh-CN",
        "enableAutomaticPunctuation": true,
        "maxAlternatives": 1,
        "profanityFilter": false,
        "enableWordTimeOffsets": true,
        "speechContexts": [
            {
                "phrases": ["example phrase"],
                "boost": 1.0
            }
        ]
    },
    "audio": {
        "content": "<base64-encoded-audio-data>"
    }
}
```

响应示例：
```json
{
    "results": [
        {
            "alternatives": [
                {
                    "transcript": "识别的文本内容",
                    "confidence": 0.98,
                    "words": [
                        {
                            "word": "识别",
                            "startTime": {
                                "seconds": 1,
                                "nanos": 100000000
                            },
                            "endTime": {
                                "seconds": 1,
                                "nanos": 300000000
                            }
                        }
                    ]
                }
            ]
        }
    ]
}
```

### 3. 长时间语音识别

```http
POST /longrunningrecognize
Content-Type: application/json
```

请求和响应格式与同步识别相同，但处理方式是异步的。

## 支持的音频格式

- WAV (Linear PCM)
- MP3
- FLAC
- OGG

## 支持的语言

- 中文 (zh-CN)
- 英文 (en-US)
- 日语 (ja-JP)
- 韩语 (ko-KR)
- 粤语 (yue-HK)

## 性能优化

- 使用 C++ 高性能实现
- 支持多线程并行处理
- 内存优化的音频处理
- 高效的模型推理

## 许可证

本项目采用 MIT 许可证。详见 LICENSE 文件。

## 贡献指南

欢迎提交 Issue 和 Pull Request。在提交 PR 之前，请确保：

1. 代码符合项目的编码规范
2. 添加了适当的测试用例
3. 更新了相关文档
4. 所有测试都通过
