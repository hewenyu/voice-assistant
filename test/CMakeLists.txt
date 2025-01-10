# 启用测试
enable_testing()

# 查找必要的包
find_package(GTest REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# 设置sherpa-onnx路径
set(SHERPA_ONNX_ROOT ${CMAKE_SOURCE_DIR}/dep/sherpa-onnx/build/install)
set(SHERPA_ONNX_LIB_DIR ${SHERPA_ONNX_ROOT}/lib)

# 添加sherpa-onnx链接目录
link_directories(${SHERPA_ONNX_LIB_DIR})

# 定义项目根目录
add_definitions(-DPROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")

# 添加语音识别测试
add_executable(voice_recognition_test voice_recognition_test.cpp)
target_link_libraries(voice_recognition_test
    PRIVATE
    voice_service_proto
    voice_service_core
    sherpa-onnx-c-api
    sherpa-onnx-cxx-api
    GTest::GTest
    GTest::Main
)

# 设置rpath
set_target_properties(voice_recognition_test PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${SHERPA_ONNX_LIB_DIR}"
)

# 添加测试
add_test(
    NAME voice_recognition_test
    COMMAND voice_recognition_test
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
) 