# Voice Assistant

基于 sherpa-onnx 的多语言语音助手，支持中文、英语、日语、韩语和粤语。

## 功能特性

- 支持多语言语音识别：
  - 中文
  - 英语
  - 日语
  - 韩语
  - 粤语
- 基于 gRPC 的服务器-客户端架构
- 使用 sherpa-onnx 作为语音识别引擎
- 支持同步和异步识别模式

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
- [x] 同步语音识别
  - [x] 音频数据处理
  - [x] 识别结果处理
- [x] 测试系统
  - [x] GTest 集成
  - [x] 多语言测试用例
  - [x] 自动化测试

### 进行中功能

- [ ] 异步语音识别
  - [ ] 任务队列管理
  - [ ] 状态追踪
  - [ ] 结果回调
- [ ] 流式语音识别
  - [ ] 实时音频处理
  - [ ] 流式结果返回
- [ ] 错误处理优化
  - [ ] 错误码规范
  - [ ] 重试机制
  - [ ] 错误恢复

### 计划功能

- [ ] 配置系统
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
- GTest (用于测试)

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
```bash
./build/src/voice_server
```

2. 运行客户端测试：
```bash
./build/src/test_client test/test_data/en.wav  # 英语测试
./build/src/test_client test/test_data/zh.wav  # 中文测试
./build/src/test_client test/test_data/ja.wav  # 日语测试
./build/src/test_client test/test_data/ko.wav  # 韩语测试
./build/src/test_client test/test_data/yue.wav # 粤语测试
```

## 测试

项目包含完整的自动化测试套件：

### 运行测试

```bash
cd build
ctest --output-on-failure
```

### 测试用例

1. 英语识别测试
   - 测试文件：`test/test_data/en.wav`
   - 预期结果：包含 "The tribal chieftain called for the boy and presented him with 50 pieces of gold"

2. 中文识别测试
   - 测试文件：`test/test_data/zh.wav`
   - 预期结果：包含 "开放时间早上9点至下午5点"

3. 日语识别测试
   - 测试文件：`test/test_data/ja.wav`
   - 预期结果：包含 "うちの中学は弁当制で持っていけない場合は50円の学校販売のパンを買う"

4. 韩语识别测试
   - 测试文件：`test/test_data/ko.wav`
   - 预期结果：包含 "조 금만 생각 을 하 면서 살 면 훨씬 편할 거야"

5. 粤语识别测试
   - 测试文件：`test/test_data/yue.wav`
   - 预期结果：包含 "呢几个字都表达唔到我想讲嘅意思"

### 测试特性

- 自动等待服务器就绪
- 详细的测试输出信息
- 准确的结果验证
- 支持 CI/CD 集成

## 项目结构

```
.
├── CMakeLists.txt          # 主CMake配置文件
├── dep/                    # 依赖库
│   └── sherpa-onnx/       # sherpa-onnx语音识别引擎
├── doc/                    # 文档
├── include/               # 头文件
├── models/                # 模型文件
├── src/                   # 源代码
│   ├── client/           # 客户端代码
│   ├── core/             # 核心实现
│   ├── proto/            # Protobuf定义
│   └── server/           # 服务器代码
└── test/                  # 测试代码和数据
    └── test_data/        # 测试音频文件
```

## 许可证

[License信息]
