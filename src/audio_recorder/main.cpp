#include "audio_capture.cpp"
#include <iostream>
#include <string>
#include <csignal>
#include <cstring>
#include <thread>
#include <chrono>

static bool running = true;

void signal_handler(int sig) {
    running = false;
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  -l, --list     List available applications\n"
              << "  -i, --index N  Record audio from application with index N\n"
              << "  -h, --help     Show this help message\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    try {
        AudioCapture capture;

        // Parse command line arguments
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
                capture.list_applications();
                const auto& apps = capture.get_available_applications();
                if (apps.empty()) {
                    std::cout << "No applications playing audio found.\n";
                }
                return 0;
            }
            else if ((strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--index") == 0) && i + 1 < argc) {
                uint32_t index = std::stoul(argv[++i]);
                capture.start_recording_application(index);
                std::cout << "Recording started. Press Ctrl+C to stop.\n";
                
                while (running) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                capture.stop_recording();
                std::cout << "\nRecording stopped.\n";
                return 0;
            }
            else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                return 0;
            }
            else {
                std::cerr << "Unknown option: " << argv[i] << "\n";
                print_usage(argv[0]);
                return 1;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 