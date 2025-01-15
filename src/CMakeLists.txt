# 收集源文件
if(WIN32)
    file(GLOB_RECURSE SOURCES
        "*.cpp"
        "*.h"
    )
else()
    file(GLOB_RECURSE SOURCES
        "*.cpp"
        "*.h"
    )
    list(FILTER SOURCES EXCLUDE REGEX ".*windows/.*")
endif()

# 创建可执行文件
add_executable(voice_assistant ${SOURCES})

# 设置包含目录
target_include_directories(voice_assistant
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/recognizer
    ${CMAKE_CURRENT_SOURCE_DIR}/translator
    ${PULSEAUDIO_INCLUDE_DIR}
)

# 链接依赖库
target_link_libraries(voice_assistant
    PRIVATE
    pulse
    pulse-simple
    ${CMAKE_THREAD_LIBS_INIT}
    ${YAML_CPP_LIBRARIES}
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    sherpa-onnx-c-api
    sherpa-onnx-cxx-api
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





