cmake_minimum_required(VERSION 3.10)

# 从环境变量或命令行参数获取版本号
if(DEFINED ENV{TAG_VERSION})
    set(VERSION_STRING $ENV{TAG_VERSION})
elseif(DEFINED TAG_VERSION)
    set(VERSION_STRING ${TAG_VERSION})
else()
    set(VERSION_STRING "1.0.0")  # 默认版本号
endif()

# 解析版本号字符串
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" VERSION_MATCH "${VERSION_STRING}")
if(VERSION_MATCH)
    set(VERSION_MAJOR ${CMAKE_MATCH_1})
    set(VERSION_MINOR ${CMAKE_MATCH_2})
    set(VERSION_PATCH ${CMAKE_MATCH_3})
else()
    message(WARNING "Invalid version format: ${VERSION_STRING}, using default version")
    set(VERSION_MAJOR 1)
    set(VERSION_MINOR 0)
    set(VERSION_PATCH 0)
endif()

project(voice_assistant 
    VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
    LANGUAGES CXX
)

# 输出当前版本信息
message(STATUS "Building version: ${PROJECT_VERSION}")

# 启用测试
enable_testing()

# 设置全局变量
set(VOICE_ASSISTANT_VERSION ${PROJECT_VERSION})
set(VOICE_ASSISTANT_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(VOICE_ASSISTANT_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(VOICE_ASSISTANT_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# 平台检测和配置
if(CMAKE_HOST_UNIX)
    # Linux 平台配置
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finders")
    find_package(PulseAudio REQUIRED)
    set(PLATFORM_NAME "linux")
elseif(CMAKE_HOST_WIN32)
    # Windows 平台配置
    set(PLATFORM_NAME "windows")
    if(NOT DEFINED MSYS64_ROOT)
        set(MSYS64_ROOT "C:/msys64")
    endif()
    message(STATUS "Using MSYS2 root: ${MSYS64_ROOT}")
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找必要的包
find_package(Threads REQUIRED)
find_package(YAML-CPP REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

# sherpa-onnx 配置
set(SHERPA_ONNX_VERSION "1.10.41")
set(SHERPA_ONNX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-prebuilt")

if(WIN32)
    set(SHERPA_ONNX_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/v${SHERPA_ONNX_VERSION}/sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared.tar.bz2")
    set(SHERPA_ONNX_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared.tar.bz2")
else()
    set(SHERPA_ONNX_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/v${SHERPA_ONNX_VERSION}/sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-x64-shared.tar.bz2")
    set(SHERPA_ONNX_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-x64-shared.tar.bz2")
endif()

# 下载和解压 sherpa-onnx
if(NOT EXISTS "${SHERPA_ONNX_ROOT}")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dep")
    
    if(NOT EXISTS "${SHERPA_ONNX_ARCHIVE}")
        message(STATUS "Downloading pre-built sherpa-onnx...")
        file(DOWNLOAD ${SHERPA_ONNX_URL} ${SHERPA_ONNX_ARCHIVE}
            SHOW_PROGRESS
            STATUS DOWNLOAD_STATUS
        )
        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download sherpa-onnx")
        endif()
    endif()
    
    message(STATUS "Extracting sherpa-onnx...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xf ${SHERPA_ONNX_ARCHIVE}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dep
    )
    
    if(WIN32)
        file(RENAME 
            "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared"
            "${SHERPA_ONNX_ROOT}"
        )
    else()
        file(RENAME 
            "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-x64-shared"
            "${SHERPA_ONNX_ROOT}"
        )
    endif()
endif()

set(SHERPA_ONNX_INCLUDE_DIR "${SHERPA_ONNX_ROOT}/include")
set(SHERPA_ONNX_LIB_DIR "${SHERPA_ONNX_ROOT}/lib")

if(NOT EXISTS "${SHERPA_ONNX_ROOT}")
    message(FATAL_ERROR "sherpa-onnx installation not found at ${SHERPA_ONNX_ROOT}")
endif()

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SHERPA_ONNX_INCLUDE_DIR}
    ${PULSEAUDIO_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

# 添加链接目录
link_directories(
    ${SHERPA_ONNX_LIB_DIR}
)

# 添加子目录
add_subdirectory(src)
add_subdirectory(test)

# 打包配置
include(InstallRequiredSystemLibraries)

if(WIN32)
    set(CPACK_GENERATOR "ZIP;NSIS")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_DISPLAY_NAME "Voice Assistant")
    set(CPACK_NSIS_PACKAGE_NAME "Voice Assistant")
    set(CPACK_NSIS_CONTACT "yuebanlaosiji@outlook.com")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "VoiceAssistant")

    # 安装 Windows 运行时依赖
    install(FILES
        "${MSYS64_ROOT}/ucrt64/bin/libyaml-cpp.dll"
        "${MSYS64_ROOT}/ucrt64/bin/libgcc_s_seh-1.dll"
        "${MSYS64_ROOT}/ucrt64/bin/libstdc++-6.dll"
        "${MSYS64_ROOT}/ucrt64/bin/libwinpthread-1.dll"
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-c-api.dll"
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-cxx-api.dll"
        "${SHERPA_ONNX_LIB_DIR}/cargs.dll"
        "${SHERPA_ONNX_LIB_DIR}/onnxruntime.dll"
        "${SHERPA_ONNX_LIB_DIR}/onnxruntime_providers_shared.dll"
        DESTINATION bin
    )
else()
    set(CPACK_GENERATOR "TGZ")
    
    # 安装 Linux 运行时依赖
    install(FILES
        "${SHERPA_ONNX_LIB_DIR}/libsherpa-onnx-c-api.so"
        "${SHERPA_ONNX_LIB_DIR}/libsherpa-onnx-cxx-api.so"
        "${SHERPA_ONNX_LIB_DIR}/libcargs.so"
        "${SHERPA_ONNX_LIB_DIR}/libonnxruntime.so"
        DESTINATION bin
    )
endif()

set(CPACK_PACKAGE_NAME "voice-assistant")
set(CPACK_PACKAGE_VENDOR "YueBan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Audio recorder with speech recognition")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${PLATFORM_NAME}")

# 安装配置文件
install(FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/config/config.${PLATFORM_NAME}.yaml"
    DESTINATION config
    RENAME config.yaml
)

# 创建并安装模型下载说明文件
file(WRITE "${CMAKE_BINARY_DIR}/MODELS_README.md"
"# 模型文件下载说明

在运行 Voice Assistant 之前，请下载以下必要的模型文件：

## 1. Sense Voice 模型（选择其中一个）

### 标准模型（更准确，文件更大）
```bash
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.onnx -O models/model.onnx
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/tokens.txt -O models/tokens.txt
```

### 量化模型（速度更快，文件更小）
```bash
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.int8.onnx -O models/model.onnx
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/tokens.txt -O models/tokens.txt
```

## 2. Whisper 模型（可选，选择其中一个）

### Tiny 模型（速度更快，准确度适中）
```bash
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.tar.bz2
tar xvf sherpa-onnx-whisper-tiny.tar.bz2 -C models/whisper/
```

### Base 模型（更准确，速度较慢）
```bash
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-base.tar.bz2
tar xvf sherpa-onnx-whisper-base.tar.bz2 -C models/whisper/
```

## 3. VAD 模型（必需）
```bash
wget https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx -O models/silero_vad.onnx
```

## 目录结构
下载完成后，请确保模型文件按以下结构放置：

```
models/
├── model.onnx          # Sense Voice 模型
├── tokens.txt          # Sense Voice tokens
├── silero_vad.onnx     # VAD 模型
└── whisper/            # 可选的 Whisper 模型
    ├── encoder.onnx
    ├── decoder.onnx
    └── tokens.txt
```

注意：请根据 config.yaml 中的配置确保模型文件路径正确。
")

# 创建空的 models 目录结构
install(CODE "
    file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/models)
    file(MAKE_DIRECTORY \${CMAKE_INSTALL_PREFIX}/models/whisper)
")

install(FILES 
    "${CMAKE_BINARY_DIR}/MODELS_README.md"
    DESTINATION .
)

include(CPack)
