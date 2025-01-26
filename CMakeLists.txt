cmake_minimum_required(VERSION 3.10)
project(voice_assistant VERSION 1.0.0 LANGUAGES CXX)

# 启用测试
enable_testing()

if(CMAKE_HOST_UNIX)
    # Add our custom CMake modules directory
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/finders")
    find_package(PulseAudio REQUIRED)
elseif(CMAKE_HOST_WIN32)
    # Windows platform doesn't use PulseAudio
    message(STATUS "Building on Windows platform")
    
    # 设置 MSYS2 根目录
    if(NOT DEFINED MSYS64_ROOT)
        set(MSYS64_ROOT "D:/msys64")
    endif()
    message(STATUS "Using MSYS2 root: ${MSYS64_ROOT}")
endif()

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

find_package(Threads REQUIRED)
find_package(YAML-CPP REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)


# set SHERPA_ONNX version
set(SHERPA_ONNX_VERSION "1.10.41")

# sherpa-onnx 依赖
if(WIN32)
    include(FetchContent)
    set(SHERPA_ONNX_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/v${SHERPA_ONNX_VERSION}/sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared.tar.bz2")
    set(SHERPA_ONNX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-prebuilt")
    set(SHERPA_ONNX_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared.tar.bz2")

    if(NOT EXISTS "${SHERPA_ONNX_ROOT}")
        # 创建目录
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dep")
        
        # 下载预编译包
        if(NOT EXISTS "${SHERPA_ONNX_ARCHIVE}")
            message(STATUS "Downloading pre-built sherpa-onnx for Windows...")
            file(DOWNLOAD ${SHERPA_ONNX_URL} ${SHERPA_ONNX_ARCHIVE}
                SHOW_PROGRESS
                STATUS DOWNLOAD_STATUS
            )
            list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
            if(NOT STATUS_CODE EQUAL 0)
                message(FATAL_ERROR "Failed to download sherpa-onnx")
            endif()
        endif()
        
        # 解压文件
        message(STATUS "Extracting sherpa-onnx...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf ${SHERPA_ONNX_ARCHIVE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dep
        )
        
        # 重命名目录
        file(RENAME 
            "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-win-x64-shared"
            "${SHERPA_ONNX_ROOT}"
        )
    endif()
else()
    # linux 平台 https://github.com/k2-fsa/sherpa-onnx/releases/download/v1.10.41/sherpa-onnx-v1.10.41-linux-x64-shared.tar.bz2
    set(SHERPA_ONNX_URL "https://github.com/k2-fsa/sherpa-onnx/releases/download/v${SHERPA_ONNX_VERSION}/sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-x64-shared.tar.bz2")
    set(SHERPA_ONNX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-prebuilt")
    set(SHERPA_ONNX_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-x64-shared.tar.bz2")

    if(NOT EXISTS "${SHERPA_ONNX_ROOT}")
        # 创建目录
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dep")
        
        # 下载预编译包
        if(NOT EXISTS "${SHERPA_ONNX_ARCHIVE}")
            message(STATUS "Downloading pre-built sherpa-onnx for Linux...")
            file(DOWNLOAD ${SHERPA_ONNX_URL} ${SHERPA_ONNX_ARCHIVE}
                SHOW_PROGRESS
                STATUS DOWNLOAD_STATUS
            )
            list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
            if(NOT STATUS_CODE EQUAL 0)
                message(FATAL_ERROR "Failed to download sherpa-onnx")
            endif()
        endif()
        
        # 解压文件
        message(STATUS "Extracting sherpa-onnx...")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E tar xf ${SHERPA_ONNX_ARCHIVE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/dep
        )
        # 重命名目录
        file(RENAME 
            "${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-v${SHERPA_ONNX_VERSION}-linux-x64-shared"
            "${SHERPA_ONNX_ROOT}"
        )
    endif()
endif()

set(SHERPA_ONNX_INCLUDE_DIR "${SHERPA_ONNX_ROOT}/include")
set(SHERPA_ONNX_LIB_DIR "${SHERPA_ONNX_ROOT}/lib")

if(NOT EXISTS "${SHERPA_ONNX_ROOT}")
    message(FATAL_ERROR "sherpa-onnx installation not found at ${SHERPA_ONNX_ROOT}")
endif()

# 设置包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-prebuilt/include
    ${PULSEAUDIO_INCLUDE_DIR}
    ${YAML_CPP_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)

# 添加链接目录
link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/sherpa-onnx-prebuilt/lib
)

# 添加子目录
add_subdirectory(src)
add_subdirectory(test)

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
