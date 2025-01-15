# 语音助手

基于 Linux 的语音助手，使用 PulseAudio 捕获应用程序音频，支持语音活动检测（VAD）、语音识别和实时翻译功能。

## 功能特点

- 基于 PulseAudio 的应用程序音频捕获（Linux 平台）
- 实时语音活动检测（VAD）
- 使用 Sherpa-onnx 进行语音识别
- 自动语言检测
- 实时翻译支持
- 针对语音识别优化的 16kHz 采样率
- 自动音频格式转换（立体声转单声道、重采样）

## 依赖项

- PulseAudio 开发库（`libpulse-dev`）- 仅限 Linux
- Sherpa-onnx
- C++17 编译器
- CMake 3.10 或更高版本

## 构建

```bash
mkdir -p build
cd build
cmake ..
make
```

## 使用方法

### 列出可用音频源

```bash
./voice_assistant -l
```

这将显示当前正在播放音频的应用程序列表，以及它们的音频槽（sink input）索引。

### 启动语音助手

```bash
./voice_assistant -s <音频槽索引> -m config.yaml
```

参数说明：
- `-s, --source <索引>`: 指定要捕获的音频槽索引
- `-m, --model <路径>`: 模型配置文件的路径
- `-l, --list`: 列出可用的音频源

## 配置说明

### 模型配置（config.yaml）

```yaml
provider: "cpu"
num_threads: 4
debug: false

model:
  type: "sense_voice"
  sense_voice:
    model_path: "models/model.onnx"
    tokens_path: "models/tokens.txt"
    language: "auto"  # 支持 "zh"、"en"、"ja"、"ko"、"yue" 等
    decoding_method: "greedy_search"
    use_itn: true

vad:
  model_path: "models/silero_vad.onnx"
  threshold: 0.5  # 语音检测阈值（0.0-1.0）
  min_silence_duration: 0.5  # 最小静音持续时间（秒）
  min_speech_duration: 0.25  # 最小语音片段持续时间（秒）
  max_speech_duration: 30.0  # 最大语音片段持续时间（秒）
  window_size: 512  # 处理窗口大小（样本数）
  sample_rate: 16000  # 采样率（Hz）
  num_threads: 1  # VAD 处理线程数
  debug: false  # 是否启用调试输出

translator:
  type: "deeplx"  # 翻译服务类型
  target_language: "zh"  # 目标翻译语言
```

## 输出格式

语音助手的识别结果输出格式如下：
```
[Recognition Result]
Time: 1.234s -- 2.345s
Text: <识别的文本>
Language Code: <检测到的语言>
Target Language: <目标语言>
Translated Text: <翻译文本>（仅当检测语言与目标语言不同时显示）
--------------------------------------------------
```

## 实现细节

### 音频捕获
- 使用 PulseAudio 的线程化主循环实现高效音频捕获
- 自动格式转换：
  - 立体声转单声道
  - 采样率转换为 16kHz
  - 16 位 PCM 格式

### 语音活动检测
- 使用 Sherpa-onnx 提供的 Silero VAD 模型
- 可配置的静音/语音持续时间参数
- 基于窗口的实时处理

### 语音识别
- 通过 Sherpa-onnx 支持多种模型类型
- 实时处理检测到的语音片段
- 支持语言检测和识别文本时间戳

### 翻译功能
- 通过 DeepLX 支持实时翻译
- 自动语言检测和翻译
- 可配置目标语言

## 注意事项

- 语音识别需要适当的模型文件和配置
- 可以使用 Ctrl+C 停止应用程序
- 确保目标应用程序正在播放音频才能捕获
- 使用 `-l` 选项查看可用的音频源
- 仅当检测到的语言与目标语言不同时才进行翻译 