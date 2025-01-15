# Collect source files
if(WIN32)
    file(GLOB_RECURSE SOURCES
        "*.cpp"
        "*.h"
    )
    list(FILTER SOURCES EXCLUDE REGEX ".*linux_pulease/.*")
else()
    file(GLOB_RECURSE SOURCES
        "*.cpp"
        "*.h"
    )
    list(FILTER SOURCES EXCLUDE REGEX ".*windows/.*")
endif()

# Create executable
add_executable(voice_assistant ${SOURCES})

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





