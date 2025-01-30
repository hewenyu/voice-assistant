# 测试模块的 CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 设置包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${SHERPA_ONNX_INCLUDE_DIR}
)

# 启用测试
enable_testing()

# 查找 GTest 包
find_package(GTest REQUIRED)

# 音频模块测试
add_executable(test_core_audio
    test_audio_capture.cpp
)

target_link_libraries(test_core_audio
    PRIVATE
    core_audio
    GTest::GTest
    GTest::Main
)

# 语音识别模块测试
add_executable(test_core_recognizer
    test_recognizer.cpp
)

target_link_libraries(test_core_recognizer
    PRIVATE
    core_recognizer
    GTest::GTest
    GTest::Main
)

# 翻译模块测试
add_executable(test_core_translator
    test_translator.cpp
)

target_link_libraries(test_core_translator
    PRIVATE
    core_translator
    GTest::GTest
    GTest::Main
)

# 添加测试
add_test(NAME test_core_audio COMMAND test_core_audio)
add_test(NAME test_core_recognizer COMMAND test_core_recognizer)
add_test(NAME test_core_translator COMMAND test_core_translator)

# Windows 特定配置
if(WIN32)
    # 设置运行时路径
    set_tests_properties(
        test_core_audio
        test_core_recognizer
        test_core_translator
        PROPERTIES
        ENVIRONMENT "PATH=${CMAKE_BINARY_DIR}/bin;$<TARGET_FILE_DIR:test_core_audio>;$ENV{PATH}"
    )
else()
    # Linux 特定配置
    set_tests_properties(
        test_core_audio
        test_core_recognizer
        test_core_translator
        PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/lib:$<TARGET_FILE_DIR:test_core_audio>:$ENV{LD_LIBRARY_PATH}"
    )
endif()

# 复制测试数据
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test_data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})