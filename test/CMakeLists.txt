# 测试模块的 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 启用测试
enable_testing()

# 查找 GTest 包
find_package(GTest REQUIRED)

# 音频模块测试
add_executable(test_audio_capture
    test_audio_capture.cpp
)

target_link_libraries(test_audio_capture
    PRIVATE
    GTest::GTest
    GTest::Main
    core_audio
)

# 语音识别模块测试
add_executable(test_recognizer
    test_recognizer.cpp
)

target_link_libraries(test_recognizer
    PRIVATE
    GTest::GTest
    GTest::Main
    core_recognizer
)

# 翻译模块测试
add_executable(test_translator
    test_translator.cpp
)

target_link_libraries(test_translator
    PRIVATE
    GTest::GTest
    GTest::Main
    core_translator
)

# 添加测试
add_test(NAME test_audio_capture COMMAND test_audio_capture)
add_test(NAME test_recognizer COMMAND test_recognizer)
add_test(NAME test_translator COMMAND test_translator)

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