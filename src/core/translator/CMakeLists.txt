# 翻译模块的 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 翻译库
add_library(core_translator STATIC
    translator.h
    deeplx_translator.cpp
    deeplx_translator.h
)

# 设置包含目录
target_include_directories(core_translator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置编译选项
target_compile_features(core_translator PRIVATE cxx_std_17)

# 查找 CURL 库
find_package(CURL REQUIRED)

# 链接依赖库
target_link_libraries(core_translator PRIVATE
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
) 