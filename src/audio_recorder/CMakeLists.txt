# Enable PulseAudio support by default
option(ENABLE_PULSEAUDIO "Enable PulseAudio support" ON)

if(NOT ENABLE_PULSEAUDIO)
    message(FATAL_ERROR "PulseAudio is required for audio recording")
    return()
endif()

# Find PulseAudio package
find_package(PulseAudio REQUIRED)

# Create audio recorder library
add_library(audio_recorder_lib
    audio_capture.cpp
)

# Set up library
target_link_libraries(audio_recorder_lib 
    PUBLIC 
    PulseAudio::PulseAudio
)

target_include_directories(audio_recorder_lib 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Create executable
add_executable(audio_recorder 
    main.cpp
)

# Link executable with library
target_link_libraries(audio_recorder 
    PRIVATE 
    audio_recorder_lib
)

# Install targets
install(TARGETS audio_recorder
    RUNTIME DESTINATION bin
)

# Add test if needed
#add_subdirectory(test) 