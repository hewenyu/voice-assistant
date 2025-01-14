# Find required packages
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(yaml-cpp REQUIRED)

# Set sherpa-onnx paths
set(SHERPA_ONNX_ROOT ${CMAKE_SOURCE_DIR}/dep/sherpa-onnx/build/install)
set(SHERPA_ONNX_INCLUDE_DIR ${SHERPA_ONNX_ROOT}/include)
set(SHERPA_ONNX_LIB_DIR ${SHERPA_ONNX_ROOT}/lib)

# Add sherpa-onnx link directory
link_directories(${SHERPA_ONNX_LIB_DIR})

# Add sherpa-onnx include directory
include_directories(${SHERPA_ONNX_INCLUDE_DIR})





