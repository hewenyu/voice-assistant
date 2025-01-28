# 设置CMake最低版本要求
cmake_minimum_required(VERSION 3.10)

# 启用测试
enable_testing()

# 查找GTest包（包含GMock）
find_package(GTest REQUIRED)

# 添加测试可执行文件
add_executable(test_audio_capture test_audio_capture.cpp)
add_executable(test_message_bus test_message_bus.cpp)

# 链接测试库
target_link_libraries(test_audio_capture
    PRIVATE
    GTest::GTest
    GTest::Main
    audio_capture
)

target_link_libraries(test_message_bus
    PRIVATE
    GTest::GTest
    GTest::Main
    GTest::gmock
    core
)

# 包含目录
target_include_directories(test_audio_capture
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(test_message_bus
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# 添加测试
add_test(NAME test_audio_capture COMMAND test_audio_capture)
add_test(NAME test_message_bus COMMAND test_message_bus)

# 设置工作目录
set_tests_properties(test_audio_capture PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(test_message_bus PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})