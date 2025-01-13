cmake_minimum_required(VERSION 3.10)

# 设置源文件
set(COMMON_SOURCES
    common/wav_writer.cpp
)

# 添加头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

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
)

# 链接依赖库
target_link_libraries(audio_recorder
    ${PLATFORM_LIBS}
)

# 安装规则
install(TARGETS audio_recorder
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/audio_recorder
    FILES_MATCHING PATTERN "*.h"
) 