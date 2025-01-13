#include "audio_capture.cpp"
#include <fstream>
#include <chrono>
#include <thread>
#include <csignal>
#include <iostream>

static bool running = true;

void signal_handler(int signal) {
    running = false;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <application_name>" << std::endl;
        return 1;
    }

    // Set up signal handler for clean exit
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create audio capture instance
    AudioCapture capture(argv[1]);
    
    // Open output file
    std::ofstream outfile("output.raw", std::ios::binary);
    if (!outfile) {
        std::cerr << "Failed to open output file" << std::endl;
        return 1;
    }

    // Start recording
    if (!capture.start()) {
        std::cerr << "Failed to start recording" << std::endl;
        return 1;
    }

    std::cout << "Recording started. Press Ctrl+C to stop." << std::endl;

    // Recording loop
    const size_t BUFFER_SIZE = 4096;  // 4KB buffer
    while (running) {
        auto buffer = capture.capture_audio(BUFFER_SIZE);
        if (!buffer.empty()) {
            outfile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));  // Small sleep to prevent busy waiting
    }

    // Clean up
    capture.stop();
    outfile.close();

    std::cout << "\nRecording stopped." << std::endl;
    return 0;
} 