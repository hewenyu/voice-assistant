cmake_minimum_required(VERSION 3.10)
project(voice_assistant VERSION 1.0.0 LANGUAGES CXX)

# Enable testing
enable_testing()

# Add our custom CMake modules directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finders")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Platform specific dependencies
if(WIN32)
    # Windows specific dependencies
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
else()
    # Linux specific dependencies
    find_package(PulseAudio REQUIRED)
endif()

# Common dependencies
find_package(Threads REQUIRED)
find_package(YAML-CPP REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(CURL REQUIRED)

# sherpa-onnx dependencies
set(SHERPA_ONNX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx/build/install")
set(SHERPA_ONNX_INCLUDE_DIR "${SHERPA_ONNX_ROOT}/include")
set(SHERPA_ONNX_LIB_DIR "${SHERPA_ONNX_ROOT}/lib")

if(NOT EXISTS "${SHERPA_ONNX_ROOT}")
    message(FATAL_ERROR "sherpa-onnx installation not found at ${SHERPA_ONNX_ROOT}")
endif()

# Include directories
set(COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${YAML_CPP_INCLUDE_DIR}
    ${SHERPA_ONNX_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

if(WIN32)
    set(PLATFORM_INCLUDE_DIRS "")
else()
    set(PLATFORM_INCLUDE_DIRS ${PULSEAUDIO_INCLUDE_DIR})
endif()

include_directories(
    ${COMMON_INCLUDE_DIRS}
    ${PLATFORM_INCLUDE_DIRS}
)

# Add link directories
link_directories(${SHERPA_ONNX_LIB_DIR})

# Add subdirectories
add_subdirectory(src)

# Package configuration
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "voice-assistant")
set(CPACK_PACKAGE_VENDOR "YueBan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Audio recorder with speech recognition")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

if(NOT WIN32)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpulse0, libcurl4, libyaml-cpp0.6")
endif()

include(CPack)