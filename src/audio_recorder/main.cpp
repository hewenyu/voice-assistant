#include <iostream>
#include <string>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <core/model_config.h>
#include "audio_capture.cpp"
#include "common/deeplx_translator.h"

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
              << "  -m, --model <path>        Use speech recognition model with YAML config at path\n"
              << "  -t, --test-translation    Test translation configuration\n"
              << "  -h, --help                Show this help message\n"
              << "\nExamples:\n"
              << "  audio_recorder --list\n"
              << "  audio_recorder -s 1 -f recording.raw\n"
              << "  audio_recorder -s 1 -m config.yaml\n"
              << "  audio_recorder -s 1 -f recording.raw -m config.yaml\n"
              << "  audio_recorder -m config.yaml -t\n"
              << "\nYAML Configuration Example:\n"
              << "  model:\n"
              << "    type: sense_voice  # or whisper\n"
              << "    sense_voice:  # if type is sense_voice\n"
              << "      model_path: path/to/model.onnx\n"
              << "      tokens_path: path/to/tokens.txt\n"
              << "      language: auto\n"
              << "    whisper:  # if type is whisper\n"
              << "      encoder_path: path/to/encoder.onnx\n"
              << "      decoder_path: path/to/decoder.onnx\n"
              << "      tokens_path: path/to/tokens.txt\n"
              << "      language: auto\n"
              << "  vad:\n"
              << "    model_path: path/to/vad.onnx\n"
              << "    threshold: 0.3\n"
              << "  deeplx:\n"
              << "    enabled: true\n"
              << "    url: http://localhost:1188/translate\n"
              << "    token: your_access_token\n"
              << "    target_lang: ZH\n";
}


// Function to test translation
void test_translation(const voice::ModelConfig& config) {
    if (!config.deeplx.enabled) {
        std::cout << "Translation is not enabled in config\n";
        return;
    }

    try {
        voice::DeepLXTranslator translator(voice::DeepLXTranslator::Config{
            config.deeplx.url,
            config.deeplx.token,
            config.deeplx.target_lang
        });

        // Test cases with different languages
        std::vector<std::pair<std::string, std::string>> test_cases = {
            {"Hello, world!", "EN"},
            {"こんにちは、世界！", "JA"},
            {"你好，世界！", "ZH"},
            {"안녕하세요, 세계!", "KO"}
        };

        std::cout << "Testing translation functionality...\n\n";
        for (const auto& test : test_cases) {
            try {
                std::cout << "Source text (" << test.second << "): " << test.first << "\n";
                std::string translated = translator.translate(test.first, test.second);
                std::cout << "Translated text: " << translated << "\n\n";
            } catch (const std::exception& e) {
                std::cout << "Translation failed: " << e.what() << "\n\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize translator: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    bool list_sources = false;
    int source_index = -1;
    std::string output_file = "output.raw";
    std::string model_config_path;
    bool test_translation_flag = false;
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
                mode = model_config_path.empty() ? OutputMode::FILE : OutputMode::BOTH;
            }
        } else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) {
                model_config_path = argv[++i];
                mode = output_file == "output.raw" ? OutputMode::MODEL : OutputMode::BOTH;
            }
        } else if (arg == "-t" || arg == "--test-translation") {
            test_translation_flag = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        }
    }

    try {
        // Load model configuration if provided
        voice::ModelConfig model_config;
        if (!model_config_path.empty()) {
            try {
                model_config = voice::ModelConfig::LoadFromFile(model_config_path);
                std::string error = model_config.Validate();
                if (!error.empty()) {
                    throw std::runtime_error("Invalid model configuration: " + error);
                }
            } catch (const std::exception& e) {
                std::cerr << "Error loading model configuration: " << e.what() << std::endl;
                return 1;
            }
        }

        // Test translation if requested
        if (test_translation_flag) {
            test_translation(model_config);
            return 0;
        }

        // Create AudioCapture instance with appropriate mode
        AudioCapture capture(model_config_path, mode);

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