#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <chrono>
#include "audio_capture.cpp"

static bool running = true;

void signal_handler(int signal) {
    running = false;
}

void print_usage() {
    std::cout << "Usage: audio_recorder [OPTIONS]\n"
              << "Options:\n"
              << "  -l, --list                List available audio sources\n"
              << "  -s, --source <index>      Record from the specified source index\n"
              << "  -f, --file <path>         Save audio to file (default: output.raw)\n"
              << "  -m, --model <path>        Use speech recognition model with config at path\n"
              << "  -h, --help                Show this help message\n"
              << "\nExamples:\n"
              << "  audio_recorder --list\n"
              << "  audio_recorder -s 1 -f recording.raw\n"
              << "  audio_recorder -s 1 -m config.json\n"
              << "  audio_recorder -s 1 -f recording.raw -m config.json\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    bool list_sources = false;
    int source_index = -1;
    std::string output_file = "output.raw";
    std::string model_config;
    OutputMode mode = OutputMode::FILE;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-l" || arg == "--list") {
            list_sources = true;
        } else if (arg == "-s" || arg == "--source") {
            if (i + 1 < argc) {
                source_index = std::stoi(argv[++i]);
            }
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 < argc) {
                output_file = argv[++i];
                mode = model_config.empty() ? OutputMode::FILE : OutputMode::BOTH;
            }
        } else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) {
                model_config = argv[++i];
                mode = output_file == "output.raw" ? OutputMode::MODEL : OutputMode::BOTH;
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        }
    }

    try {
        // Create AudioCapture instance with appropriate mode
        AudioCapture capture(model_config, mode);

        if (list_sources) {
            capture.list_applications();
            return 0;
        }

        if (source_index < 0) {
            std::cerr << "Error: No source specified. Use -s or --source to specify a source index.\n";
            return 1;
        }

        // Set up signal handler
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        // Start recording
        capture.start_recording_application(source_index, output_file);
        std::cout << "Recording started. Press Ctrl+C to stop.\n";

        // Wait for signal
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop recording
        capture.stop_recording();
        std::cout << "\nRecording stopped.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
} 