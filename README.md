# 音频录制器

linux 平台基于 PulseAudio 的应用程序音频录制工具，支持语音活动检测（VAD）和语音识别功能。

## 功能特点

- 使用 PulseAudio 录制特定应用程序的音频
- 支持 WAV 和原始音频输出格式
- 实时语音活动检测（VAD）
- 使用 Sherpa-onnx 进行语音识别
- 针对语音识别优化的 16kHz 采样率
- 自动音频格式转换（立体声转单声道、重采样）

## 依赖项

- PulseAudio 开发库（`libpulse-dev`） linux 限定
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
./audio_recorder -l
```

这将显示当前正在播放音频的应用程序列表，以及它们的音频槽（sink input）索引。

### 使用语音识别处理音频

```bash
./audio_recorder -s <音频槽索引> -m config.yaml
```

- `-m, --model <路径>`: 使用指定配置文件进行语音识别


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
```

## 实现细节

### 音频捕获

- 使用 PulseAudio 的线程化主循环实现高效音频捕获
- 支持自动格式转换：
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

## 输出格式

使用语音识别时，输出格式如下：
```
Time: 1.234s -- 2.345s
Text: 识别的文本内容
Language: 检测到的语言（如果有）
--------------------------------------------------
```

## 注意事项

- WAV 输出始终使用 16 位 PCM 格式
- 语音识别需要适当的模型文件和配置
- 可以使用 Ctrl+C 停止应用程序
- 确保目标应用程序正在播放音频才能录制
- 录制前建议先使用 `-l` 选项查看可用的音频源 