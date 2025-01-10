# C API 使用指南

本文档描述了如何使用 sherpa-onnx 的 C API 进行语音识别。

## 构建说明

### 1. 下载模型

首先，我们需要下载预训练的模型：

```bash
curl -SL -O https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
tar xvf sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
rm sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17.tar.bz2
```

### 2. 构建 sherpa-onnx

```bash
cd dep/sherpa-onnx
mkdir build
cd build

# 配置构建选项
cmake \
  -D CMAKE_BUILD_TYPE=Release \
  -D BUILD_SHARED_LIBS=ON \
  -D CMAKE_INSTALL_PREFIX=./install \
  -D SHERPA_ONNX_ENABLE_BINARY=OFF \
  ..

# 编译并安装
make -j2 install
```

构建完成后，你应该能在 `build/install` 目录下看到以下文件：

- Linux 系统：
  ```
  install/lib/libonnxruntime.so
  install/lib/libsherpa-onnx-c-api.so
  install/include/sherpa-onnx/c-api/c-api.h
  ```

- macOS 系统：
  ```
  install/lib/libonnxruntime.1.17.1.dylib
  install/lib/libonnxruntime.dylib
  install/lib/libsherpa-onnx-c-api.dylib
  install/include/sherpa-onnx/c-api/c-api.h
  ```

### 3. 编译使用 C API 的程序

以下是一个简单的编译命令示例：

```bash
gcc -o your-program your-program.c \
  -I ./dep/sherpa-onnx/build/install/include \
  -L ./dep/sherpa-onnx/build/install/lib/ \
  -l sherpa-onnx-c-api \
  -l onnxruntime
```

说明：
- `-I` 选项添加头文件搜索路径
- `-L` 选项添加库文件搜索路径
- `-l` 选项指定需要链接的库

### 4. 运行程序

在运行程序之前，需要设置库文件搜索路径：

Linux 系统：
```bash
export LD_LIBRARY_PATH=$PWD/dep/sherpa-onnx/build/install/lib:$LD_LIBRARY_PATH
```

macOS 系统：
```bash
export DYLD_LIBRARY_PATH=$PWD/dep/sherpa-onnx/build/install/lib:$DYLD_LIBRARY_PATH
```

然后运行你的程序：
```bash
./your-program
```

## API 使用示例

你可以参考 sherpa-onnx 提供的示例代码：
[sense-voice-c-api.c](https://github.com/k2-fsa/sherpa-onnx/blob/master/c-api-examples/sense-voice-c-api.c)

## 注意事项

1. 由于使用了共享库，运行程序时必须正确设置 `LD_LIBRARY_PATH`（Linux）或 `DYLD_LIBRARY_PATH`（macOS），否则会出现运行时错误。

2. 建议始终使用最新版本的 sherpa-onnx。

3. 编译时需要确保系统已安装 ONNX Runtime。

4. 在生产环境中使用时，建议将模型文件和库文件放在固定的位置，并相应地修改程序中的路径。

## 支持的功能

- 实时语音识别
- 批量音频文件识别
- 多语言支持（中文、英文、日语、韩语、粤语）
- 自动标点
- 词级别时间戳

## 故障排除

1. 找不到库文件
   - 检查 `LD_LIBRARY_PATH` 或 `DYLD_LIBRARY_PATH` 是否正确设置
   - 确认库文件是否存在于指定路径

2. 找不到头文件
   - 检查 `-I` 参数指定的路径是否正确
   - 确认头文件是否存在于指定路径

3. 运行时错误
   - 检查模型文件路径是否正确
   - 确认 ONNX Runtime 版本兼容性
   - 查看程序日志获取详细错误信息 