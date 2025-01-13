cmake_minimum_required(VERSION 3.10)

# 设置源文件
set(COMMON_SOURCES
    common/wav_writer.cpp
    common/deeplx_translator.cpp
)

# 添加头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..    # 添加 src 目录到包含路径
)

# 查找 yaml-cpp 库
find_package(yaml-cpp REQUIRED)

# 查找 nlohmann-json
find_package(nlohmann_json 3.2.0 REQUIRED)

# 根据平台选择实现
if(WIN32)
    set(PLATFORM_SOURCES
        windows/wasapi_capture.cpp
    )
    
    # Windows特定依赖
    set(PLATFORM_LIBS
        ole32      # COM支持
        oleaut32   # COM类型库支持
    )
else()
    set(PLATFORM_SOURCES
        linux/pulse_audio_capture.cpp
    )
    
    # Linux特定依赖
    find_package(PulseAudio REQUIRED)
    set(PLATFORM_LIBS
        ${PULSEAUDIO_LIBRARIES}
    )
    include_directories(${PULSEAUDIO_INCLUDE_DIRS})
endif()

# 创建库
add_library(audio_recorder STATIC
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
    audio_capture.cpp    # 添加 IAudioCapture 的实现
)

# 链接依赖库
target_link_libraries(audio_recorder
    ${PLATFORM_LIBS}
    voice_service_core
    nlohmann_json::nlohmann_json
)

# 创建可执行文件
add_executable(audio_recorder_app
    main.cpp
)

# 链接audio_recorder库和yaml-cpp
target_link_libraries(audio_recorder_app
    audio_recorder
    yaml-cpp
    voice_service_core
    ${PULSEAUDIO_LIBRARIES}
    pulse
    pulse-simple
    nlohmann_json::nlohmann_json
)

# 安装规则
install(TARGETS audio_recorder audio_recorder_app
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/audio_recorder
    FILES_MATCHING PATTERN "*.h"
) 