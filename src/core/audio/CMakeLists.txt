# 音频模块的 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 音频捕获库
add_library(core_audio STATIC
    audio_capture.cpp
    $<$<PLATFORM_ID:Windows>:windows/wasapi_capture.cpp>
    $<$<PLATFORM_ID:Linux>:linux/pulse_audio_capture.cpp>
)

# 设置音频捕获库的包含目录
target_include_directories(core_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/linux
)

# 设置音频捕获库的编译选项
target_compile_features(core_audio PUBLIC cxx_std_17)

# Windows 平台特定配置
if(WIN32)
    target_link_libraries(core_audio PUBLIC
        ole32
        oleaut32
        winmm
        ksuser
    )
endif()

# Linux 平台特定配置
if(UNIX)
    find_package(PulseAudio REQUIRED)
    target_link_libraries(core_audio PUBLIC
        ${PULSEAUDIO_LIBRARIES}
    )
endif() 