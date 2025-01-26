# 收集源文件
if(WIN32)
    file(GLOB_RECURSE SOURCES
        "*.cpp"
        "*.h"
    )
    # 排除 Linux 相关文件
    list(FILTER SOURCES EXCLUDE REGEX ".*linux_pulease/.*")
else()
    file(GLOB_RECURSE SOURCES
        "*.cpp"
        "*.h"
    )
    # 排除 Windows 相关文件
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
    ${SHERPA_ONNX_INCLUDE_DIR}
)

# 链接依赖库
if(WIN32)
    target_link_libraries(voice_assistant
        PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        ${YAML_CPP_LIBRARIES}
        nlohmann_json::nlohmann_json
        ${CURL_LIBRARIES}
        sherpa-onnx-c-api
        sherpa-onnx-cxx-api
        audio_capture
        ole32
        oleaut32
        winmm
        ksuser
        avrt
    )

    # 复制所有需要的 DLL 到可执行文件目录
    add_custom_command(TARGET voice_assistant POST_BUILD
        # 复制 audio_capture DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/bin/libaudio_capture.dll"
            $<TARGET_FILE_DIR:voice_assistant>
    )

    add_custom_command(TARGET voice_assistant POST_BUILD
        # 复制 yaml-cpp DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MSYS64_ROOT}/ucrt64/bin/libyaml-cpp.dll"
            $<TARGET_FILE_DIR:voice_assistant>
    )

    add_custom_command(TARGET voice_assistant POST_BUILD
        # 复制 sherpa-onnx DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-c-api.dll"
            "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-cxx-api.dll"
            "${SHERPA_ONNX_LIB_DIR}/cargs.dll"
            "${SHERPA_ONNX_LIB_DIR}/onnxruntime.dll"
            "${SHERPA_ONNX_LIB_DIR}/onnxruntime_providers_shared.dll"
            $<TARGET_FILE_DIR:voice_assistant>
    )

    # 复制 MSYS2 运行时 DLL
    add_custom_command(TARGET voice_assistant POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MSYS64_ROOT}/ucrt64/bin/libgcc_s_seh-1.dll"
            "${MSYS64_ROOT}/ucrt64/bin/libstdc++-6.dll"
            "${MSYS64_ROOT}/ucrt64/bin/libwinpthread-1.dll"
            $<TARGET_FILE_DIR:voice_assistant>
    )
else()
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
        audio_capture
    )
endif()

# 安装配置
install(TARGETS voice_assistant
    RUNTIME DESTINATION bin
)

if(WIN32)
    # 安装DLL依赖
    file(GLOB DLL_FILES "${CMAKE_BINARY_DIR}/bin/*.dll")
    install(FILES ${DLL_FILES}
        DESTINATION bin
    )
endif()


# 安装配置文件
install(FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/../config/config.yaml.template"
    DESTINATION etc/voice_assistant
    RENAME config.yaml
)

# 创建音频捕获库
set(AUDIO_CAPTURE_SOURCES
    audio/audio_capture.cpp
)

if(WIN32)
    list(APPEND AUDIO_CAPTURE_SOURCES
        audio/windows/wasapi_capture.cpp
    )
else()
    list(APPEND AUDIO_CAPTURE_SOURCES
        audio/linux_pulease/pulse_audio_capture.cpp
    )
endif()

add_library(audio_capture SHARED ${AUDIO_CAPTURE_SOURCES})

# 添加导出宏
target_compile_definitions(audio_capture
    PRIVATE
    AUDIO_CAPTURE_EXPORTS
    PUBLIC
    AUDIO_CAPTURE_DLL
)

# 设置包含目录
target_include_directories(audio_capture
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Windows 特定配置
if(WIN32)
    target_include_directories(audio_capture
        PUBLIC
        ${SHERPA_ONNX_INCLUDE_DIR}
    )

    target_link_libraries(audio_capture
        PUBLIC
        ole32
        oleaut32
        winmm
        ksuser
        avrt
        ${YAML_CPP_LIBRARIES}
        sherpa-onnx-c-api
    )
# Linux 特定配置
else()
    target_include_directories(audio_capture
        PUBLIC
        ${PULSEAUDIO_INCLUDE_DIR}
    )

    target_link_libraries(audio_capture
        PUBLIC
        ${PULSEAUDIO_LIBRARIES}
        ${YAML_CPP_LIBRARIES}
        sherpa-onnx-c-api
    )
endif()

# 设置输出目录
set_target_properties(audio_capture PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)





