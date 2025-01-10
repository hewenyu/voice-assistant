# Find required packages
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Set protobuf options
set(PROTOBUF_USE_STATIC_LIBS ON)

# Set sherpa-onnx paths
set(SHERPA_ONNX_ROOT ${CMAKE_SOURCE_DIR}/dep/sherpa-onnx/build/install)
set(SHERPA_ONNX_INCLUDE_DIR ${SHERPA_ONNX_ROOT}/include)
set(SHERPA_ONNX_LIB_DIR ${SHERPA_ONNX_ROOT}/lib)

# Add sherpa-onnx link directory
link_directories(${SHERPA_ONNX_LIB_DIR})

# Generate protobuf files
get_filename_component(voice_service_proto "proto/voice_service.proto" ABSOLUTE)
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

# Create a library for the proto files
add_library(voice_service_proto
    ${hw_proto_srcs}
    ${hw_proto_hdrs}
    ${GRPC_SRCS}
    ${GRPC_HDRS}
)

target_link_libraries(voice_service_proto
    PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
)

target_include_directories(voice_service_proto
    PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Create a library for the core implementation
add_library(voice_service_core
    core/voice_service_impl.cpp
)

target_link_libraries(voice_service_core
    PUBLIC
    voice_service_proto
    sherpa-onnx-c-api
    sherpa-onnx-cxx-api
)

target_include_directories(voice_service_core
    PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${SHERPA_ONNX_INCLUDE_DIR}
)

# Add server executable
add_executable(voice_server
    server/main.cpp
)

target_link_libraries(voice_server
    PRIVATE
    voice_service_core
    gRPC::grpc++_reflection
)

# Add client executable
add_executable(test_client
    client/test_client.cpp
)

target_link_libraries(test_client
    PRIVATE
    voice_service_proto
)

# Set rpath for server
set_target_properties(voice_server PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${SHERPA_ONNX_LIB_DIR}"
) 