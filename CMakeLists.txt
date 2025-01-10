cmake_minimum_required(VERSION 3.13)
project(voice-assistant)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Set sherpa-onnx paths
set(SHERPA_ONNX_ROOT ${CMAKE_SOURCE_DIR}/dep/sherpa-onnx/build/install)
set(SHERPA_ONNX_INCLUDE_DIR ${SHERPA_ONNX_ROOT}/include)
set(SHERPA_ONNX_LIB_DIR ${SHERPA_ONNX_ROOT}/lib)

# Generate protobuf files
get_filename_component(voice_service_proto "src/proto/voice_service.proto" ABSOLUTE)
get_filename_component(voice_service_proto_path "${voice_service_proto}" PATH)

# Generate protobuf files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${voice_service_proto})
get_filename_component(hw_proto_srcs "${PROTO_SRCS}" ABSOLUTE)
get_filename_component(hw_proto_hdrs "${PROTO_HDRS}" ABSOLUTE)

# Generate gRPC files
set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/voice_service.grpc.pb.cc")
set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/voice_service.grpc.pb.h")
add_custom_command(
    OUTPUT "${GRPC_SRCS}" "${GRPC_HDRS}"
    COMMAND protobuf::protoc
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
         --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
         -I "${voice_service_proto_path}"
         --plugin=protoc-gen-grpc=/usr/bin/grpc_cpp_plugin
         "${voice_service_proto}"
    DEPENDS "${voice_service_proto}"
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SHERPA_ONNX_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

# Link directories
link_directories(${SHERPA_ONNX_LIB_DIR})

# Add server executable
add_executable(voice_server
    src/server/main.cpp
    src/core/voice_service_impl.cpp
    ${hw_proto_srcs}
    ${hw_proto_hdrs}
    ${GRPC_SRCS}
    ${GRPC_HDRS}
)

# Link libraries
target_link_libraries(voice_server
    sherpa-onnx-c-api
    sherpa-onnx-cxx-api
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
)

# Set rpath
set_target_properties(voice_server PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${SHERPA_ONNX_LIB_DIR}"
) 