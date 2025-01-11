#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <grpcpp/grpcpp.h>
#include "voice_service.grpc.pb.h"
#include "sherpa-onnx/c-api/c-api.h"

// Convert float samples to int16 samples and then to string
std::string SamplesToString(const float* samples, int32_t n) {
    // Convert to int16
    std::vector<int16_t> int16_samples(n);
    for (int32_t i = 0; i < n; ++i) {
        float sample = samples[i];
        // Clamp to [-1, 1]
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        // Convert to int16
        int16_samples[i] = static_cast<int16_t>(sample * 32767.0f);
    }
    
    return std::string(
        reinterpret_cast<const char*>(int16_samples.data()),
        n * sizeof(int16_t)
    );
}

// Preprocess audio file using sox
std::string PreprocessAudio(const std::string& input_file) {
    std::string output_file = input_file + ".16k.wav";
    std::string cmd = "sox \"" + input_file + "\" -r 16000 -c 1 -b 16 \"" + output_file + "\"";
    
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Failed to preprocess audio file" << std::endl;
        return "";
    }
    
    return output_file;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <audio_file> <vad_model>" << std::endl;
        return 1;
    }

    const char* audio_file = argv[1];
    const char* vad_model = argv[2];

    // Preprocess audio file
    std::string processed_file = PreprocessAudio(audio_file);
    if (processed_file.empty()) {
        return 1;
    }

    // Create gRPC channel
    auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    std::unique_ptr<VoiceService::Stub> stub = VoiceService::NewStub(channel);

    // Read audio file using sherpa-onnx API
    const SherpaOnnxWave* wave = SherpaOnnxReadWave(processed_file.c_str());
    if (!wave) {
        std::cerr << "Failed to read audio file: " << processed_file << std::endl;
        return 1;
    }

    if (wave->sample_rate != 16000) {
        std::cerr << "Expected sample rate: 16000, got: " << wave->sample_rate << std::endl;
        SherpaOnnxFreeWave(wave);
        return 1;
    }

    // Initialize VAD
    SherpaOnnxVadModelConfig vadConfig = {};  // Zero initialization
    vadConfig.silero_vad.model = vad_model;
    vadConfig.silero_vad.threshold = 0.3;  // Lower threshold for more sensitive detection
    vadConfig.silero_vad.min_silence_duration = 0.25;  // Shorter silence duration
    vadConfig.silero_vad.min_speech_duration = 0.1;  // Shorter minimum speech duration
    vadConfig.silero_vad.max_speech_duration = 15;  // Shorter maximum speech duration
    vadConfig.silero_vad.window_size = 256;  // Smaller window size for finer granularity
    vadConfig.sample_rate = 16000;
    vadConfig.num_threads = 1;
    vadConfig.debug = 1;  // Enable debug output

    SherpaOnnxVoiceActivityDetector* vad = 
        SherpaOnnxCreateVoiceActivityDetector(&vadConfig, 30);
    if (!vad) {
        std::cerr << "Failed to create VAD" << std::endl;
        SherpaOnnxFreeWave(wave);
        return 1;
    }

    // Process audio with VAD
    int32_t window_size = vadConfig.silero_vad.window_size;
    int32_t i = 0;
    bool is_eof = false;
    bool was_speech = false;  // Track previous speech state
    float last_speech_end = 0;

    while (!is_eof) {
        if (i + window_size < wave->num_samples) {
            SherpaOnnxVoiceActivityDetectorAcceptWaveform(
                vad, wave->samples + i, window_size);
        } else {
            SherpaOnnxVoiceActivityDetectorFlush(vad);
            is_eof = true;
        }

        bool is_speech = SherpaOnnxVoiceActivityDetectorDetected(vad);
        
        // State transition from non-speech to speech
        if (is_speech && !was_speech) {
            std::cout << "Speech started at: " << (i / 16000.0f) << "s" << std::endl;
        }
        
        // State transition from speech to non-speech
        if (!is_speech && was_speech) {
            std::cout << "Speech ended at: " << (i / 16000.0f) << "s" << std::endl;
            
            while (!SherpaOnnxVoiceActivityDetectorEmpty(vad)) {
                const SherpaOnnxSpeechSegment* segment = 
                    SherpaOnnxVoiceActivityDetectorFront(vad);
                
                float current_start = segment->start / 16000.0f;
                float current_end = (segment->start + segment->n) / 16000.0f;
                
                // Only process if this is a new sentence (gap > 0.3s)
                if ((current_start - last_speech_end) > 0.3) {
                    std::cout << "Processing speech segment: " << current_start 
                             << "s -> " << current_end << "s" << std::endl;

                    // Create gRPC request for this segment
                    SyncRecognizeRequest request;
                    request.set_audio_data(
                        SamplesToString(segment->samples, segment->n));

                    // Call RPC
                    SyncRecognizeResponse response;
                    grpc::ClientContext context;
                    grpc::Status status = stub->SyncRecognize(&context, request, &response);

                    if (status.ok()) {
                        std::cout << "[" << current_start << "s -> " << current_end 
                                 << "s] " << response.text() << std::endl;
                        last_speech_end = current_end;
                    } else {
                        std::cerr << "RPC failed: " << status.error_message() << std::endl;
                    }
                }

                SherpaOnnxDestroySpeechSegment(segment);
                SherpaOnnxVoiceActivityDetectorPop(vad);
            }
        }
        
        was_speech = is_speech;
        i += window_size;
    }

    // Cleanup
    SherpaOnnxDestroyVoiceActivityDetector(vad);
    SherpaOnnxFreeWave(wave);
    
    // Remove temporary file
    std::remove(processed_file.c_str());

    return 0;
} 