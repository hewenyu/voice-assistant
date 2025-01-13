# Enable PulseAudio support by default
option(ENABLE_PULSEAUDIO "Enable PulseAudio support" ON)

if(NOT ENABLE_PULSEAUDIO)
    message(FATAL_ERROR "PulseAudio is required for audio recording")
    return()
endif()

# Find PulseAudio package
find_package(PulseAudio REQUIRED)

# Create audio recorder executable
add_executable(audio_recorder
    main.cpp
)

# Include directories
target_include_directories(audio_recorder
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PULSEAUDIO_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(audio_recorder
    PRIVATE
    voice_service_core
    ${PULSEAUDIO_LIBRARIES}
    pthread
)

# Install targets
install(TARGETS audio_recorder
    RUNTIME DESTINATION bin
)

# Add test if needed
#add_subdirectory(test) 