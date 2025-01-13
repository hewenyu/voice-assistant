cmake_minimum_required(VERSION 3.10)
project(voice_assistant)

# 启用测试
enable_testing()

# Add our custom CMake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finders")

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(nlohmann_json 3.2.0 REQUIRED)

# 添加子目录
add_subdirectory(src)
add_subdirectory(test) 