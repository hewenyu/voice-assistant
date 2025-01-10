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

## 开发进度

### 已完成功能

- [x] 项目基础架构搭建
- [x] gRPC 服务定义
- [x] sherpa-onnx 集成
- [x] 同步语音识别接口实现
- [x] 流式语音识别接口实现
- [x] 基础错误处理

### 进行中功能

- [ ] 音频数据处理
  - [ ] base64 编解码
  - [ ] 音频格式转换
  - [ ] 采样率转换
- [ ] 异步语音识别
  - [ ] 任务队列管理
  - [ ] 状态追踪
  - [ ] 结果回调

### 待开发功能

- [ ] 配置系统
  - [ ] 配置文件支持
  - [ ] 动态配置更新
  - [ ] 多模型支持
- [ ] 日志系统
  - [ ] 分级日志
  - [ ] 日志轮转
  - [ ] 性能日志
- [ ] 错误处理
  - [ ] 错误码规范
  - [ ] 重试机制
  - [ ] 优雅降级
- [ ] 性能优化
  - [ ] 内存池
  - [ ] 线程池
  - [ ] 批处理优化
- [ ] 监控指标
  - [ ] 性能指标收集
  - [ ] 健康检查
  - [ ] 资源使用监控
- [ ] 文档完善
  - [ ] API 文档
  - [ ] 部署文档
  - [ ] 开发指南

## 依赖项

- C++17 或更高版本
- CMake 3.10 或更高版本
- sherpa-onnx（作为子模块）
- gRPC
- Protobuf
- ONNX Runtime
- libsndfile

## 构建说明

1. 克隆项目并初始化子模块：
```bash
git clone https://github.com/hewenyu/voice-assistant.git
cd voice-assistant
git submodule sync && git submodule update --init --recursive
```


## 使用说明

### 启动服务

```bash
./build/voice_server
```

默认监听 0.0.0.0:50051

### API 接口

#### 1. 同步识别

适用于短音频（<1分钟）的识别。

```protobuf
service VoiceService {
  rpc SyncRecognize(SyncRecognizeRequest) returns (SyncRecognizeResponse);
}
```

#### 2. 异步识别

适用于长音频（<480分钟）的识别。

```protobuf
service VoiceService {
  rpc AsyncRecognize(AsyncRecognizeRequest) returns (AsyncRecognizeResponse);
  rpc GetAsyncRecognizeStatus(GetAsyncRecognizeStatusRequest) returns (AsyncRecognizeResponse);
}
```

#### 3. 流式识别

适用于实时音频流的识别。

```protobuf
service VoiceService {
  rpc StreamingRecognize(stream StreamingRecognizeRequest) returns (stream StreamingRecognizeResponse);
}
```

## 贡献指南

欢迎提交 Issue 和 Pull Request。在提交 PR 之前，请确保：

1. 代码符合项目的编码规范
2. 添加了适当的测试用例
3. 更新了相关文档
4. 所有测试都通过

## 许可证

本项目采用 MIT 许可证。详见 LICENSE 文件。
