# 收集源文件
file(GLOB_RECURSE SOURCES
    "*.cpp"
    "*.h"
)

# 创建可执行文件
add_executable(voice_assistant ${SOURCES})

# 链接依赖库
target_link_libraries(voice_assistant
    PRIVATE
    ${PULSEAUDIO_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${YAML_CPP_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    sherpa-onnx-c-api
    sherpa-onnx-core
)

# 设置包含目录
target_include_directories(voice_assistant
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 安装配置
install(TARGETS voice_assistant
    RUNTIME DESTINATION bin
)

# 安装配置文件
install(FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/../config.yaml.example"
    DESTINATION etc/voice_assistant
    RENAME config.yaml
)





