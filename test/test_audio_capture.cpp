#include <iostream>
#include <thread>
#include <chrono>
#include <audio/audio_capture.h>

int main() {
    try {
        // Create audio capture instance
        auto audio_capture = audio::IAudioCapture::CreateAudioCapture();
        
        // Initialize
        if (!audio_capture->initialize()) {
            std::cerr << "Failed to initialize audio capture" << std::endl;
            return 1;
        }

        std::cout << "\nListing available audio applications..." << std::endl;
        audio_capture->list_applications();

        // 为了测试目的，我们不需要等待用户输入
        unsigned int app_id = 14272;  // 使用系统音频
        
        // Start recording
        if (!audio_capture->start_recording_application(app_id)) {
            std::cerr << "Failed to start recording" << std::endl;
            return 1;
        }

        // 为了测试目的，只录制几秒钟
        std::cout << "Recording for 3 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        // Stop recording
        audio_capture->stop_recording();
        std::cout << "Recording stopped" << std::endl;

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 