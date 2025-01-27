// main.cpp

#include <iostream>
#include <memory>
#include <csignal>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

#include <common/model_config.h>
#include <audio/audio_capture.h>
#include <translator/translator.h>
#include <sherpa-onnx/c-api/c-api.h>
#include <recognizer/model_factory.h>

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT) {
        g_running = false;
    }
}

void print_usage() {
    std::cout << "Usage: audio_recorder [OPTIONS]\n"
              << "Options:\n"
              << "  -l, --list                List available audio sources\n"
              << "  -s, --source <index>      Record from the specified source index\n"
              << "  -m, --model <path>        Use speech recognition model with YAML config at path\n"
              << "  -h, --help                Show this help message\n"
              << "\nExamples:\n"
              << "  audio_recorder --list\n"
              << "  audio_recorder -s 1 -m config.yaml\n"
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

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    // 如果没有参数，显示帮助信息并退出
    if (argc == 1) {
        print_usage();
        return 0;
    }

    // Register signal handler for graceful shutdown
    std::signal(SIGINT, signal_handler);

    bool list_sources = false;
    int source_index = -1;
    std::string model_config_path;

    // parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-l" || arg == "--list") {
            list_sources = true;
        } else if (arg == "-s" || arg == "--source") {
            if (i + 1 < argc) {
                source_index = std::stoi(argv[++i]);
            }
        } else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) {
                model_config_path = argv[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage();
            return 1;
        }
    }

    if (list_sources) {
        std::cout << "Listing available audio sources..." << std::endl;
        // Create audio capture instance
        auto audio_capture = audio::IAudioCapture::CreateAudioCapture();
        if (!audio_capture) {
            std::cerr << "Failed to create audio capture instance" << std::endl;
            return 1;
        }
        std::cout << "Audio capture instance created successfully" << std::endl;

        if (!audio_capture->initialize()) {
            std::cerr << "Failed to initialize audio capture" << std::endl;
            return 1;
        }
        std::cout << "Audio capture initialized successfully" << std::endl;

        std::cout << "\nAvailable audio sources:" << std::endl;
        audio_capture->list_applications();
        return 0;
    }

    try {
        
        // Load model configuration
        common::ModelConfig model_config;
        if (!model_config_path.empty()) {
            model_config = common::ModelConfig::LoadFromFile(model_config_path);
        } else if (!list_sources) {
            std::cerr << "Model configuration is required for speech recognition." << std::endl;
            return 1;
        }

        // Create audio capture instance
        auto audio_capture = audio::IAudioCapture::CreateAudioCapture();
        if (!audio_capture) {
            std::cerr << "Failed to create audio capture instance." << std::endl;
            return 1;
        }

        if (!audio_capture->initialize()) {
            std::cerr << "Failed to initialize audio capture." << std::endl;
            return 1;
        }

        // create recognizer
        auto recognizer = recognizer::ModelFactory::CreateModel(model_config);
        if (!recognizer) {
            std::cerr << "Failed to create speech recognizer." << std::endl;
            return 1;
        }

        // create VAD
        auto vad = recognizer::ModelFactory::CreateVoiceActivityDetector(model_config);
        if (!vad) {
            std::cerr << "Failed to create VAD." << std::endl;
            return 1;
        }

        // Set VAD first
        audio_capture->set_model_vad(vad, model_config.vad.window_size);
        
        // Then set recognizer
        audio_capture->set_model_recognizer(recognizer);

        // create translator
        auto translator = translator::CreateTranslator(translator::TranslatorType::DeepLX, model_config);
        if (!translator) {
            std::cerr << "Failed to create translator." << std::endl;
            return 1;
        }
        
        audio_capture->set_translate(translator.get());

        if (source_index < 0) {
            std::cerr << "Please specify a valid source index with -s option." << std::endl;
            return 1;
        }

        // Set up signal handler
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        // Start audio capture
        if (!audio_capture->start_recording_application(source_index)) {
            std::cerr << "Failed to start audio capture." << std::endl;
            return 1;
        }

        // Main processing loop
        while (g_running) {
            // Sleep for a short duration to prevent busy-waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cleanup
        audio_capture->stop_recording();
        std::cout << "\nRecording stopped.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
