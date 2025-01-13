#include "audio_capture.cpp"
#include <iostream>
#include <string>
#include <csignal>
#include <cstring>
#include <thread>
#include <chrono>
#include <fstream>

static bool running = true;

void signal_handler(int sig) {
    running = false;
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  -l, --list                List available applications\n"
              << "  -i, --index N             Record audio from application with index N\n"
              << "  -c, --config <file>       Load configuration from file\n"
              << "  -o, --output <file>       Save audio to file (raw 16kHz mono S16LE format)\n"
              << "  -h, --help                Show this help message\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::string config_file;
    std::string output_file = "output.raw";
    bool list_apps = false;
    int app_index = -1;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-l" || arg == "--list") {
            list_apps = true;
        } else if ((arg == "-i" || arg == "--index") && i + 1 < argc) {
            app_index = std::stoi(argv[++i]);
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_file = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    try {
        // Create audio capture instance with configuration
        AudioCapture capture(config_file);

        if (list_apps) {
            capture.list_applications();
            const auto& apps = capture.get_available_applications();
            if (apps.empty()) {
                std::cout << "No applications playing audio found.\n";
            }
            return 0;
        }

        if (app_index >= 0) {
            // Open output file
            std::ofstream outfile(output_file, std::ios::binary);
            if (!outfile) {
                std::cerr << "Failed to open output file: " << output_file << std::endl;
                return 1;
            }

            capture.start_recording_application(app_index);
            std::cout << "Recording started. Press Ctrl+C to stop.\n";
            
            // Recording loop
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Get and save audio data
                auto audio_data = capture.get_audio_data();
                if (!audio_data.empty()) {
                    outfile.write(reinterpret_cast<const char*>(audio_data.data()),
                                audio_data.size() * sizeof(int16_t));
                    capture.clear_audio_data();
                }
            }
            
            capture.stop_recording();
            outfile.close();
            std::cout << "\nRecording stopped. Audio saved to " << output_file << std::endl;
            return 0;
        }

        std::cerr << "No action specified. Use -h for help.\n";
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 