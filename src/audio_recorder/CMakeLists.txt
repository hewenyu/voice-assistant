# Enable PulseAudio support by default
option(ENABLE_PULSEAUDIO "Enable PulseAudio support" ON)

if(NOT ENABLE_PULSEAUDIO)
    message(FATAL_ERROR "PulseAudio is required for audio recording")
    return()
endif()

# Find PulseAudio package
find_package(PulseAudio REQUIRED)

add_executable(audio_recorder
    main.cpp
)

target_link_libraries(audio_recorder
    PRIVATE
    ${PULSEAUDIO_LIBRARIES}
    pthread
)

target_include_directories(audio_recorder
    PRIVATE
    ${PULSEAUDIO_INCLUDE_DIRS}
)

# Install targets
install(TARGETS audio_recorder
    RUNTIME DESTINATION bin
)

# Add test if needed
#add_subdirectory(test) 