# 收集源文件
file(GLOB_RECURSE COMMON_SOURCES
    "common/*.cpp"
    "common/*.h"
    "recognizer/*.cpp"
    "recognizer/*.h"
    "translator/*.cpp"
    "translator/*.h"
    "utills/*.cpp"
    "utills/*.h"
)

if(WIN32)
    file(GLOB_RECURSE PLATFORM_SOURCES
        "audio/windows/*.cpp"
        "audio/windows/*.h"
    )
else()
    file(GLOB_RECURSE PLATFORM_SOURCES
        "audio/linux_pulease/*.cpp"
        "audio/linux_pulease/*.h"
    )
endif()

set(SOURCES
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
    "main.cpp"
    "audio/audio_capture.cpp"
)

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
    ${SHERPA_ONNX_INCLUDE_DIR}
)

# 创建音频捕获库
add_library(audio_capture SHARED
    "audio/audio_capture.cpp"
    ${PLATFORM_SOURCES}
)

# 添加导出宏
target_compile_definitions(audio_capture
    PRIVATE
    AUDIO_CAPTURE_EXPORTS
    PUBLIC
    AUDIO_CAPTURE_DLL
)

# 设置音频捕获库的包含目录
target_include_directories(audio_capture
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SHERPA_ONNX_INCLUDE_DIR}
)

# 平台特定配置
if(WIN32)
    # Windows 特定链接
    target_link_libraries(voice_assistant
        PRIVATE
        audio_capture
        sherpa-onnx-c-api
        sherpa-onnx-cxx-api
        ${CMAKE_THREAD_LIBS_INIT}
        ${YAML_CPP_LIBRARIES}
        nlohmann_json::nlohmann_json
        ${CURL_LIBRARIES}
        ole32
        oleaut32
        winmm
        ksuser
        avrt
    )

    target_link_libraries(audio_capture
        PUBLIC
        sherpa-onnx-c-api
        ${YAML_CPP_LIBRARIES}
        ole32
        oleaut32
        winmm
        ksuser
        avrt
    )

    # 复制运行时依赖
    set(RUNTIME_DEPS
        "${CMAKE_BINARY_DIR}/bin/libaudio_capture.dll"
        "${MSYS64_ROOT}/ucrt64/bin/libyaml-cpp.dll"
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-c-api.dll"
        "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-cxx-api.dll"
        "${SHERPA_ONNX_LIB_DIR}/cargs.dll"
        "${SHERPA_ONNX_LIB_DIR}/onnxruntime.dll"
        "${SHERPA_ONNX_LIB_DIR}/onnxruntime_providers_shared.dll"
        "${MSYS64_ROOT}/ucrt64/bin/libgcc_s_seh-1.dll"
        "${MSYS64_ROOT}/ucrt64/bin/libstdc++-6.dll"
        "${MSYS64_ROOT}/ucrt64/bin/libwinpthread-1.dll"
    )

    foreach(DEP ${RUNTIME_DEPS})
        add_custom_command(TARGET voice_assistant POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DEP}
                $<TARGET_FILE_DIR:voice_assistant>
        )
    endforeach()

else()
    # Linux 特定链接
    target_link_libraries(voice_assistant
        PRIVATE
        audio_capture
        sherpa-onnx-c-api
        sherpa-onnx-cxx-api
        ${CMAKE_THREAD_LIBS_INIT}
        ${YAML_CPP_LIBRARIES}
        nlohmann_json::nlohmann_json
        ${CURL_LIBRARIES}
        pulse
        pulse-simple
    )

    target_link_libraries(audio_capture
        PUBLIC
        sherpa-onnx-c-api
        ${YAML_CPP_LIBRARIES}
        ${PULSEAUDIO_LIBRARIES}
    )
endif()

# 设置输出目录
set_target_properties(audio_capture PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# 安装配置
install(TARGETS voice_assistant audio_capture
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装配置文件
install(FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/../config/config.yaml.template"
    DESTINATION etc/voice_assistant
    RENAME config.yaml
)