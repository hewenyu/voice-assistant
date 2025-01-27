# Add test executables
add_executable(test_audio_capture 
    test_audio_capture.cpp
)

# Set include directories
target_include_directories(test_audio_capture
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${SHERPA_ONNX_INCLUDE_DIR}
)

# Link with audio capture library
target_link_libraries(test_audio_capture
    PRIVATE
    audio_capture
    ${YAML_CPP_LIBRARIES}
)

# Windows 特定配置
if(WIN32)
    target_link_libraries(test_audio_capture
        PRIVATE
        ole32
        oleaut32
        winmm
        ksuser
        avrt
    )

    # Windows 测试配置
    add_test(NAME test_audio_capture 
        COMMAND ${CMAKE_COMMAND} -E env 
            "PATH=${CMAKE_BINARY_DIR}/bin;$<TARGET_FILE_DIR:test_audio_capture>;$ENV{PATH}" 
            $<TARGET_FILE:test_audio_capture>
    )
# Linux 特定配置
else()
    target_link_libraries(test_audio_capture
        PRIVATE
        ${PULSEAUDIO_LIBRARIES}
    )

    # Linux 测试配置
    add_test(NAME test_audio_capture 
        COMMAND ${CMAKE_COMMAND} -E env 
            "PATH=${CMAKE_BINARY_DIR}/bin;$<TARGET_FILE_DIR:test_audio_capture>;$ENV{PATH}"
            "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/bin:$<TARGET_FILE_DIR:test_audio_capture>:$ENV{LD_LIBRARY_PATH}"
            $<TARGET_FILE:test_audio_capture>
    )
endif() 

# 设置测试属性
set_tests_properties(test_audio_capture PROPERTIES
    ENVIRONMENT "VOICE_ASSISTANT_TEST=1"
)