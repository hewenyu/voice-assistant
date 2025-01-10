cmake_minimum_required(VERSION 3.10)
project(voice_assistant)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖包
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# 设置 ONNX Runtime 路径
set(ONNXRUNTIME_ROOT_DIR "/usr/local" CACHE PATH "ONNX Runtime root directory")
set(ONNXRUNTIME_INCLUDE_DIRS "${ONNXRUNTIME_ROOT_DIR}/include")
set(ONNXRUNTIME_LIB_DIRS "${ONNXRUNTIME_ROOT_DIR}/lib")

# 添加 sherpa-onnx 子目录
add_subdirectory(dep/sherpa-onnx)

# 生成 protobuf 和 gRPC 文件
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
execute_process(
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
        --plugin=protoc-gen-grpc="${grpc_cpp_plugin_location}"
        -I "${CMAKE_CURRENT_SOURCE_DIR}/src/proto"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/proto/voice_service.proto"
)

# 添加生成的文件
set(PROTO_SRCS
    "${CMAKE_CURRENT_BINARY_DIR}/voice_service.pb.cc"
)
set(PROTO_HDRS
    "${CMAKE_CURRENT_BINARY_DIR}/voice_service.pb.h"
)
set(GRPC_SRCS
    "${CMAKE_CURRENT_BINARY_DIR}/voice_service.grpc.pb.cc"
)
set(GRPC_HDRS
    "${CMAKE_CURRENT_BINARY_DIR}/voice_service.grpc.pb.h"
)

# 添加源文件
set(SOURCES
    src/core/voice_service_impl.cpp
    src/server/main.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# 创建可执行文件
add_executable(voice_server ${SOURCES})

# 包含目录
target_include_directories(voice_server PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Protobuf_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx/build/install/include
)

# 链接库
target_link_libraries(voice_server PRIVATE
    ${Protobuf_LIBRARIES}
    gRPC::grpc++
    gRPC::grpc
    onnxruntime
    sherpa-onnx-c-api
) 