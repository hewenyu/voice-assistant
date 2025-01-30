# Core 模块的 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 添加子目录
add_subdirectory(audio)
add_subdirectory(recognizer)
add_subdirectory(translator)

# 设置库的输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) 