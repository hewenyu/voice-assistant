# 语音识别模块的 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 语音识别库
add_library(core_recognizer STATIC
    sherpa_recognizer.cpp
)

# 设置语音识别库的包含目录
target_include_directories(core_recognizer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SHERPA_ONNX_INCLUDE_DIR}
)

# 设置语音识别库的编译选项
target_compile_features(core_recognizer PUBLIC cxx_std_17)

# 链接依赖库
target_link_libraries(core_recognizer PUBLIC
    sherpa-onnx-c-api
    sherpa-onnx-cxx-api
) 