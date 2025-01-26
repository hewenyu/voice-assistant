# Add test executables
add_executable(test_audio_capture 
    test_audio_capture.cpp
)

# Link with audio capture library
target_link_libraries(test_audio_capture
    PRIVATE
    audio_capture
)

# Windows 特定配置
if(WIN32)
    target_link_libraries(test_audio_capture
        PRIVATE
        ole32
        oleaut32
        winmm
        ksuser
        avrt
    )

    # 复制所有需要的 DLL 到测试目录
    add_custom_command(TARGET test_audio_capture POST_BUILD
        # 复制 audio_capture DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/bin/libaudio_capture.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    add_custom_command(TARGET test_audio_capture POST_BUILD
        # 复制 yaml-cpp DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MSYS64_ROOT}/ucrt64/bin/libyaml-cpp.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    add_custom_command(TARGET test_audio_capture POST_BUILD
        # 复制 sherpa-onnx DLL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-c-api.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    add_custom_command(TARGET test_audio_capture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHERPA_ONNX_LIB_DIR}/sherpa-onnx-cxx-api.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    add_custom_command(TARGET test_audio_capture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHERPA_ONNX_LIB_DIR}/cargs.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    add_custom_command(TARGET test_audio_capture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHERPA_ONNX_LIB_DIR}/onnxruntime.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    add_custom_command(TARGET test_audio_capture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SHERPA_ONNX_LIB_DIR}/onnxruntime_providers_shared.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    # 复制 MSYS2 运行时 DLL
    add_custom_command(TARGET test_audio_capture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MSYS64_ROOT}/ucrt64/bin/libgcc_s_seh-1.dll"
            "${MSYS64_ROOT}/ucrt64/bin/libstdc++-6.dll"
            "${MSYS64_ROOT}/ucrt64/bin/libwinpthread-1.dll"
            $<TARGET_FILE_DIR:test_audio_capture>
    )

    # Windows 测试配置
    add_test(NAME test_audio_capture 
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}/bin;${SHERPA_ONNX_LIB_DIR};D:/msys64/ucrt64/bin;$ENV{PATH}" 
        $<TARGET_FILE:test_audio_capture>
    )
# Linux 特定配置
else()
    target_link_libraries(test_audio_capture
        PRIVATE
        ${PULSEAUDIO_LIBRARIES}
    )

    # Linux 测试配置
    add_test(NAME test_audio_capture 
        COMMAND ${CMAKE_COMMAND} -E env "PATH=${CMAKE_BINARY_DIR}/bin;$ENV{PATH}" 
        $<TARGET_FILE:test_audio_capture>
    )
endif() 