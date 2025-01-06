cmake_minimum_required(VERSION 3.10)
project(voice_assistant VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加依赖的子模块
add_subdirectory(dep/SenseVoice.cpp)

# 查找必要的包
find_package(Threads REQUIRED)

# 添加cpp-httplib
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.1
)
FetchContent_MakeAvailable(httplib)

# 包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/dep/SenseVoice.cpp/sense-voice/csrc
    ${httplib_SOURCE_DIR}
)

# 源文件
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.cc"
)

# 可执行文件
add_executable(voice_assistant ${SOURCES})

# 链接依赖库
target_link_libraries(voice_assistant
    PRIVATE
    Threads::Threads
    sense-voice
    httplib
) 