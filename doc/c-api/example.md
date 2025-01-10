# C API 示例程序

本文档提供了一个使用 sherpa-onnx C API 的简单示例程序。

## 示例代码

```c
#include <stdio.h>
#include <stdlib.h>
#include "sherpa-onnx/c-api/c-api.h"

int main() {
    // 配置识别参数
    struct SherpaOnnxOnlineRecognizerConfig config;
    config.model_config.encoder = "path/to/encoder.onnx";
    config.model_config.decoder = "path/to/decoder.onnx";
    config.model_config.tokens = "path/to/tokens.txt";
    
    config.feat_config.sample_rate = 16000;
    config.feat_config.feature_dim = 80;
    
    config.decoder_config.decoding_method = "greedy_search";
    config.decoder_config.num_active_paths = 4;
    
    config.enable_endpoint = 1;
    config.max_active_paths = 4;
    
    // 创建识别器
    struct SherpaOnnxOnlineRecognizer *recognizer = 
        sherpa_onnx_online_recognizer_new(&config);
    
    if (!recognizer) {
        fprintf(stderr, "Failed to create recognizer\n");
        return 1;
    }
    
    // 创建流
    struct SherpaOnnxOnlineStream *stream = 
        sherpa_onnx_online_recognizer_create_stream(recognizer);
    
    if (!stream) {
        fprintf(stderr, "Failed to create stream\n");
        sherpa_onnx_online_recognizer_delete(recognizer);
        return 1;
    }
    
    // 读取音频数据并进行识别
    float samples[3200]; // 假设这里有音频数据
    
    // 处理音频数据
    sherpa_onnx_online_stream_accept_waveform(
        stream,
        16000, // 采样率
        samples,
        3200
    );
    
    // 获取识别结果
    struct SherpaOnnxOnlineRecognizerResult result = 
        sherpa_onnx_online_recognizer_decode(recognizer);
    
    // 打印结果
    printf("识别结果: %s\n", result.text);
    
    // 清理资源
    sherpa_onnx_online_recognizer_delete_stream(recognizer, stream);
    sherpa_onnx_online_recognizer_delete(recognizer);
    
    return 0;
}
```

## 编译命令

```bash
gcc -o speech-recognition speech-recognition.c \
    -I ./dep/sherpa-onnx/build/install/include \
    -L ./dep/sherpa-onnx/build/install/lib/ \
    -l sherpa-onnx-c-api \
    -l onnxruntime
```

## 运行程序

1. 设置库路径：

```bash
# Linux
export LD_LIBRARY_PATH=$PWD/dep/sherpa-onnx/build/install/lib:$LD_LIBRARY_PATH

# macOS
export DYLD_LIBRARY_PATH=$PWD/dep/sherpa-onnx/build/install/lib:$DYLD_LIBRARY_PATH
```

2. 运行程序：

```bash
./speech-recognition
```

## 代码说明

1. 配置初始化
   - 设置模型文件路径
   - 配置音频参数
   - 配置解码参数

2. 创建识别器
   - 使用配置创建识别器实例
   - 检查创建是否成功

3. 创建识别流
   - 为实时识别创建数据流
   - 检查创建是否成功

4. 处理音频
   - 接收音频数据
   - 进行识别
   - 获取结果

5. 资源清理
   - 删除流
   - 删除识别器

## 注意事项

1. 路径设置
   - 确保模型文件路径正确
   - 使用绝对路径或相对于程序的正确路径

2. 内存管理
   - 及时释放资源
   - 检查内存分配是否成功

3. 错误处理
   - 检查函数返回值
   - 适当的错误处理和日志记录

4. 音频格式
   - 确保音频数据格式正确（采样率、通道数等）
   - 正确设置音频参数 