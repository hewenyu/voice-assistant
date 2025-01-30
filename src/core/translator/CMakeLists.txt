# 翻译模块的 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 翻译库
add_library(core_translator STATIC
    deeplx_translator.cpp
)

# 设置翻译库的包含目录
target_include_directories(core_translator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 设置翻译库的编译选项
target_compile_features(core_translator PUBLIC cxx_std_17)

# 链接依赖库
target_link_libraries(core_translator PUBLIC
    nlohmann_json::nlohmann_json
    CURL::libcurl
    Threads::Threads
) 