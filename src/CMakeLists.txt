# Collect source files
set(COMMON_SOURCES
    "audio/audio_capture.cpp"
    "translator/translator.cpp"
    "translator/deepl/deeplx_translator.cpp"
    "main.cpp"
)

if(WIN32)
    set(PLATFORM_SOURCES
        "audio/windows/wasapi_capture.cpp"
    )
    
    # Find Windows SDK path
    if(DEFINED ENV{WindowsSdkDir} AND DEFINED ENV{WindowsSDKVersion})
        set(WINDOWS_SDK_INCLUDE_DIRS
            "$ENV{WindowsSdkDir}Include/$ENV{WindowsSDKVersion}um"
            "$ENV{WindowsSdkDir}Include/$ENV{WindowsSDKVersion}shared"
        )
    else()
        message(FATAL_ERROR "Windows SDK not found. Please install Windows SDK or set WindowsSdkDir and WindowsSDKVersion environment variables.")
    endif()
else()
    set(PLATFORM_SOURCES
        "audio/linux_pulease/pulse_audio_capture.cpp"
    )
endif()

# Create executable
add_executable(voice_assistant ${COMMON_SOURCES} ${PLATFORM_SOURCES})

# Set include directories
target_include_directories(voice_assistant
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/recognizer
    ${CMAKE_CURRENT_SOURCE_DIR}/translator
)

# Platform specific dependencies
if(WIN32)
    # Add Windows SDK include paths
    target_include_directories(voice_assistant PRIVATE ${WINDOWS_SDK_INCLUDE_DIRS})
    
    # Add Windows specific libraries
    target_link_libraries(voice_assistant
        PRIVATE
        ${CMAKE_THREAD_LIBS_INIT}
        CURL::libcurl
        yaml-cpp
        nlohmann_json::nlohmann_json
        sherpa-onnx-c-api
        sherpa-onnx-cxx-api
    )
else()
    target_include_directories(voice_assistant PRIVATE ${PULSEAUDIO_INCLUDE_DIR})
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
endif()

# Installation configuration
install(TARGETS voice_assistant
    RUNTIME DESTINATION bin
)

# Install config file
install(FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/../config.yaml.example"
    DESTINATION etc/voice_assistant
    RENAME config.yaml
)





