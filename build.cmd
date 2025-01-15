@echo off
setlocal enabledelayedexpansion

:: Check for required tools
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo CMake is not found in PATH
    echo Please install CMake and add it to PATH
    exit /b 1
)

where cl >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo MSVC compiler not found
    echo Please run this script from a Visual Studio Developer Command Prompt
    exit /b 1
)

:: Create and enter build directory
if not exist build (
    mkdir build
)
cd build

:: Configure with CMake
echo Configuring with CMake...
cmake -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=E:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
    -DCMAKE_BUILD_TYPE=Release ^
    ..

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed
    exit /b 1
)

:: Build the project
echo Building the project...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed
    exit /b 1
)

echo Build completed successfully
cd ..

endlocal 