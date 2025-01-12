# Voice Assistant

基于 sherpa-onnx 的多语言语音助手，支持中文、英语、日语、韩语和粤语。

## 功能特性

- 支持多语言语音识别：
  - 中文
  - 英语
  - 日语
  - 韩语
  - 粤语
- 支持多种识别模型：
  - Sense Voice（适用于 CJK 语言）
  - Whisper（适用于英语和多语言场景）
- 基于 gRPC 的服务器-客户端架构
- 使用 sherpa-onnx 作为语音识别引擎
- 支持同步和流式识别模式
- 灵活的命令行配置支持

## 最新改进

### 2024-01-12
- 添加了 Whisper 模型支持
  - 支持自动语言检测
  - 支持多语言识别
  - 优化了短音频处理
  - 提供了量化模型选项

### 2024-01-11
- 优化了 VAD 参数配置
  - 降低阈值到 0.3 提高检测灵敏度
  - 调整最小静音时长为 0.25s
  - 调整最小语音时长为 0.1s
  - 设置最大语音时长为 15s
  - 减小窗口大小到 256 提高实时性

- 改进了流式识别的语音分段
  - 实现了更精确的语音状态检测
  - 优化了中间结果的输出频率
  - 改进了语音段的开始和结束判断
  - 提供更自然的语音分段效果

## 模型下载与准备

### 下载预训练模型

```bash
# 创建模型目录
mkdir -p models/whisper

# 下载 Sense Voice 模型（以下二选一）
# 1. 标准模型（更准确，文件更大）
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.onnx -O models/model.onnx

# 2. 量化模型（速度更快，文件更小）
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.int8.onnx -O models/model.int8.onnx

# 下载 Sense Voice 词表文件
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/tokens.txt -O models/tokens.txt

# 下载 Whisper 模型（以下二选一）
# 1. tiny 模型（速度快，准确度适中）
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.tar.bz2
tar xvf sherpa-onnx-whisper-tiny.tar.bz2 -C models/whisper/
rm sherpa-onnx-whisper-tiny.tar.bz2

# 2. base 模型（准确度更高，速度较慢）
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-base.tar.bz2
tar xvf sherpa-onnx-whisper-base.tar.bz2 -C models/whisper/
rm sherpa-onnx-whisper-base.tar.bz2

# 下载 VAD 模型
wget https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx -O models/silero_vad.onnx
```

### 模型说明

1. Sense Voice 模型：
   - `model.onnx`：标准浮点模型，准确度更高
   - `model.int8.onnx`：8位量化模型，速度更快，占用空间更小
   - 主要用于中文、日语、韩语和粤语识别

2. Whisper 模型：
   - `tiny`：小型模型，速度快，适合一般场景
   - `base`：基础模型，准确度更高，适合对准确度要求高的场景
   - 支持自动语言检测
   - 适合英语和多语言混合场景

3. VAD 模型：
   - `silero_vad.onnx`：语音活动检测模型
   - 用于准确分割语音片段

## 开发进度

### 已完成功能

- [x] 项目基础架构搭建
  - [x] CMake 构建系统
  - [x] 项目目录结构
  - [x] 依赖库集成
- [x] gRPC 服务实现
  - [x] Protobuf 服务定义
  - [x] 服务器端实现
  - [x] 客户端实现
- [x] sherpa-onnx 集成
  - [x] 模型加载
  - [x] 语音识别接口
  - [x] 多语言支持
  - [x] VAD 集成
- [x] 同步语音识别
  - [x] 音频数据处理
  - [x] 识别结果处理
- [x] 流式语音识别
  - [x] 实时音频处理
  - [x] 流式结果返回
  - [x] VAD 语音分段
- [x] 配置系统
  - [x] 命令行参数支持
  - [x] 模型参数配置
  - [x] 服务器参数配置

### 进行中功能

- [ ] 异步语音识别
  - [ ] 任务队列管理
  - [ ] 状态追踪
  - [ ] 结果回调
- [ ] 错误处理优化
  - [ ] 错误码规范
  - [ ] 重试机制
  - [ ] 错误恢复

### 计划功能

- [ ] 配置系统扩展
  - [ ] 配置文件支持
  - [ ] 动态配置更新
  - [ ] 多模型配置
- [ ] 性能优化
  - [ ] 内存池
  - [ ] 线程池
  - [ ] 批处理优化
- [ ] 监控系统
  - [ ] 性能指标收集
  - [ ] 健康检查
  - [ ] 资源使用监控
- [ ] 文档完善
  - [ ] API 文档
  - [ ] 部署文档
  - [ ] 开发指南

## 依赖项

- CMake >= 3.10
- C++17 编译器
- gRPC
- Protobuf
- sherpa-onnx
- sox (用于音频预处理)
- GTest (用于测试)
- yaml-cpp

## 构建

```bash
# 构建 sherpa-onnx
cd dep/sherpa-onnx
mkdir -p build && cd build
cmake -DCMAKE_INSTALL_PREFIX=./install ..
make -j4
make install
cd ../../..

# 构建项目
mkdir -p build && cd build
cmake ..
make
```

## 运行

1. 启动服务器：

基本用法：
```bash
Usage: ./voice-assistant <config_file>  # 使用配置文件启动
   or: ./voice-assistant [options]      # 使用命令行参数启动（向后兼容）

配置文件方式:
  <config_file>            YAML 格式的配置文件路径
```

2. 运行客户端：
```bash
# VAD 测试客户端
./build/src/vad_client input.wav models/vad.onnx

# 流式识别客户端
./build/src/streaming_client input.wav

# 同步识别测试
./build/src/test_client test/test_data/en.wav  # 英语测试
./build/src/test_client test/test_data/zh.wav  # 中文测试
./build/src/test_client test/test_data/ja.wav  # 日语测试
./build/src/test_client test/test_data/ko.wav  # 韩语测试
./build/src/test_client test/test_data/yue.wav # 粤语测试
```

### 流式识别示例输出

使用流式识别客户端处理中文音频文件的输出示例：

<!-- 使用doc/asset/stream.png -->
![stream](doc/asset/stream.png)

这个示例展示了：
1. 实时的中间识别结果输出
2. 随着语音输入的增加，识别结果逐步完善
3. 最终输出完整的识别结果

## 配置说明

### 配置文件结构
```yaml
# 基础配置
provider: "cpu"          # 推理后端：cpu 或 cuda
num_threads: 4          # 线程数
debug: false           # 调试模式

# 模型配置
model:
  type: "whisper"      # 模型类型：sense_voice 或 whisper

  # Sense Voice 配置
  sense_voice:
    model_path: "models/model.onnx"
    tokens_path: "models/tokens.txt"
    language: "auto"    # zh, en, ja, ko, yue
    decoding_method: "greedy_search"
    use_itn: true

  # Whisper 配置
  whisper:
    encoder_path: "models/whisper/tiny-encoder.int8.onnx"
    decoder_path: "models/whisper/tiny-decoder.int8.onnx"
    tokens_path: "models/whisper/tiny-tokens.txt"
    language: "auto"    # auto, en, zh, ja, ko 等
    task: "transcribe"  # transcribe 或 translate
    tail_paddings: 2000 # 处理短音频的填充
    decoding_method: "greedy_search"

# VAD 配置
vad:
  model_path: "models/silero_vad.onnx"
  threshold: 0.3 
  min_silence_duration: 0.25
  min_speech_duration: 0.1
  max_speech_duration: 15
  window_size: 256
  sample_rate: 16000
```

### 配置说明

1. 基础配置
   - `provider`: 选择推理后端，支持 "cpu" 或 "cuda"
   - `num_threads`: CPU 线程数，建议设置为 CPU 核心数
   - `debug`: 是否启用调试模式，输出更多日志信息

2. 模型配置
   - `type`: 选择使用的模型类型
     - `sense_voice`: 适用于中日韩粤语识别
     - `whisper`: 适用于英语和多语言场景

3. Sense Voice 配置
   - `model_path`: 模型文件路径
   - `tokens_path`: 词表文件路径
   - `language`: 识别语言，支持 "auto"、"zh"、"en"、"ja"、"ko"、"yue"
   - `decoding_method`: 解码方法，目前支持 "greedy_search"
   - `use_itn`: 是否启用智能数字转换

4. Whisper 配置
   - `encoder_path`: 编码器模型路径
   - `decoder_path`: 解码器模型路径
   - `tokens_path`: 词表文件路径
   - `language`: 识别语言，支持 "auto" 和多种语言代码
   - `task`: 任务类型，支持 "transcribe"（转写）和 "translate"（翻译）
   - `tail_paddings`: 短音频处理的填充长度，建议设置为 2000
   - `decoding_method`: 解码方法，目前支持 "greedy_search"

5. VAD 配置
   - `model_path`: VAD 模型路径
   - `threshold`: 语音检测阈值，范围 0-1，越小越敏感
   - `min_silence_duration`: 最小静音持续时间（秒）
   - `min_speech_duration`: 最小语音持续时间（秒）
   - `max_speech_duration`: 最大语音持续时间（秒）
   - `window_size`: 处理窗口大小，影响实时性
   - `sample_rate`: 采样率，固定为 16000

### 配置示例

1. 英语识别优化配置：
```yaml
model:
  type: "whisper"
  whisper:
    encoder_path: "models/whisper/base-encoder.onnx"  # 使用 base 模型提高准确度
    decoder_path: "models/whisper/base-decoder.onnx"
    tokens_path: "models/whisper/base-tokens.txt"
    language: "en"      # 固定为英语提高性能
    tail_paddings: 2000
```

2. 中日韩识别优化配置：
```yaml
model:
  type: "sense_voice"
  sense_voice:
    model_path: "models/model.onnx"    # 使用标准模型提高准确度
    tokens_path: "models/tokens.txt"
    language: "auto"    # 自动检测语言
    use_itn: true      # 启用智能数字转换
```

3. 实时性能优化配置：
```yaml
provider: "cpu"
num_threads: 4         # 根据 CPU 核心数调整

model:
  type: "whisper"
  whisper:
    encoder_path: "models/whisper/tiny-encoder.int8.onnx"  # 使用量化模型
    decoder_path: "models/whisper/tiny-decoder.int8.onnx"
    language: "auto"

vad:
  threshold: 0.3
  min_silence_duration: 0.2  # 缩短静音判断时间
  window_size: 256          # 使用较小的窗口
```

## 许可证

本项目采用 [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) 开源协议。


## 快速开始

### 下载预编译包

访问 [Releases](https://github.com/hewenyu/voice-assistant/releases) 页面，下载最新的 `voice-assistant-linux.tar.gz` 文件。

```bash
# 使用 curl 下载最新版本
curl -L https://github.com/hewenyu/voice-assistant/releases/latest/download/voice-assistant-linux.tar.gz -o voice-assistant-linux.tar.gz

# 解压下载的文件
tar xzf voice-assistant-linux.tar.gz

# 进入解压后的目录
cd linux

# 添加可执行权限
chmod +x ./bin/*

# 设置库文件路径（重要：每次新开终端都需要执行）
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH

# 创建配置文件
cp config/config.yaml.template config/config.yaml
# 根据需要修改配置文件
vim config/config.yaml

# 创建模型目录
mkdir -p models

# 下载模型文件（以下二选一）
# 1. 标准模型（更准确，文件更大）
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.onnx -O models/model.onnx

# 2. 量化模型（速度更快，文件更小）
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.int8.onnx -O models/model.int8.onnx

# 下载词表文件
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/tokens.txt -O models/tokens.txt

# 下载 VAD 模型
wget https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx -O models/vad.onnx 

# 启动服务器
./bin/voice_server -m models/model.onnx -t models/tokens.txt -v models/vad.onnx 
```

## 配置系统说明

### 配置文件格式

配置文件位于 `config/config.yaml`，使用 YAML 格式。主要包含以下配置项：

#### 基础配置

```yaml
provider: "cpu"      # 推理后端，可选值: "cpu", "cuda", "coreml" 等
num_threads: 4       # 使用的 CPU 线程数
debug: false        # 是否启用调试模式
```

#### 模型配置

```yaml
model:
  type: "sense_voice"  # 模型类型，目前支持 "sense_voice"
  sense_voice:
    model_path: "models/model.onnx"      # 模型文件路径
    tokens_path: "models/tokens.txt"      # 词表文件路径
    language: "auto"                      # 语言，可选值: "auto", "zh", "en" 等
    decoding_method: "greedy_search"      # 解码方法，可选值: "greedy_search", "beam_search"
    use_itn: true                         # 是否使用 ITN (Inverse Text Normalization)
```

#### VAD (语音活动检测) 配置

```yaml
vad:
  model_path: "models/silero_vad.onnx"  # VAD 模型文件路径
  threshold: 0.3                        # VAD 检测阈值，范围 [0,1]，越小越敏感
  min_silence_duration: 0.25            # 最小静音持续时间（秒）
  min_speech_duration: 0.1              # 最小语音持续时间（秒）
  max_speech_duration: 15               # 最大语音持续时间（秒）
  window_size: 256                      # 窗口大小，必须为 2 的幂
  sample_rate: 16000                    # 音频采样率（Hz）
```

### 参数详细说明

#### VAD 参数调优指南

- `threshold`: 
  - 作用：控制语音检测的灵敏度
  - 值越小，VAD 越容易检测到语音
  - 值越大，误检率越低，但可能漏检
  - 建议范围：0.2-0.5
  - 默认值：0.3（经过优化的平衡值）

- `min_silence_duration`:
  - 作用：控制语音片段的切分
  - 检测到的静音必须持续这么长才会触发语音片段结束
  - 值越小，语音片段切分越频繁
  - 值越大，更容易将多个语音片段合并为一个
  - 默认值：0.25秒（适合正常语速）

- `min_speech_duration`:
  - 作用：过滤短促噪音
  - 检测到的语音必须持续这么长才会被认为是有效语音
  - 值越小，越容易检测到短音
  - 值越大，能更好地过滤掉噪声和杂音
  - 默认值：0.1秒（可以捕捉到短音节）

- `max_speech_duration`:
  - 作用：防止语音片段过长
  - 单个语音片段的最大长度
  - 超过此长度会强制切分
  - 建议根据实际使用场景调整
  - 默认值：15秒（适合大多数对话场景）

#### 性能相关参数

- `num_threads`: 
  - 作用：控制计算资源使用
  - 推荐设置为 CPU 核心数
  - 值越大，计算速度越快，但内存占用也越大
  - 默认值：4（适合大多数设备）

- `window_size`:
  - 作用：控制 VAD 的实时性和准确性
  - 影响 VAD 的实时性和计算量
  - 值越小，延迟越低，但可能影响准确性
  - 必须为 2 的幂（如 256, 512, 1024）
  - 默认值：256（优化后的平衡值）

### 配置文件使用方法

1. 创建配置文件：
```bash
cp config/config.yaml.template config/config.yaml
```

2. 根据需求修改配置：
```bash
# 编辑配置文件
vim config/config.yaml
```

3. 使用配置文件运行：
```bash
./voice-assistant config/config.yaml
```

### 配置优化建议

1. 语音检测优化：
   - 嘈杂环境：提高 threshold 到 0.4-0.5
   - 安静环境：可以降低 threshold 到 0.2-0.3
   - 快速响应：减小 min_silence_duration 和 min_speech_duration
   - 准确性优先：增加 min_speech_duration

2. 性能优化：
   - 高性能设备：增加 num_threads，减小 window_size
   - 低性能设备：减少 num_threads，适当增加 window_size
   - 实时性要求高：减小 window_size
   - 准确性要求高：增加 window_size
