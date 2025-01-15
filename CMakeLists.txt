cmake_minimum_required(VERSION 3.10)
project(voice_assistant VERSION 1.0.0 LANGUAGES CXX)

# 启用测试
enable_testing()

# Add our custom CMake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finders")

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# 查找必要的包
find_package(PulseAudio REQUIRED)
find_package(Threads REQUIRED)
find_package(YAML-CPP REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(CURL REQUIRED)

# sherpa-onnx 依赖
set(SHERPA_ONNX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx/build/install")
set(SHERPA_ONNX_INCLUDE_DIR "${SHERPA_ONNX_ROOT}/include")
set(SHERPA_ONNX_LIB_DIR "${SHERPA_ONNX_ROOT}/lib")

if(NOT EXISTS "${SHERPA_ONNX_ROOT}")
    message(FATAL_ERROR "sherpa-onnx installation not found at ${SHERPA_ONNX_ROOT}")
endif()

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PULSEAUDIO_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
    ${SHERPA_ONNX_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

# 添加链接目录
link_directories(${SHERPA_ONNX_LIB_DIR})

# 添加子目录
add_subdirectory(src)

# 打包配置
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "voice-assistant")
set(CPACK_PACKAGE_VENDOR "YueBan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Audio recorder with speech recognition")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpulse0, libcurl4, libyaml-cpp0.6")

include(CPack)