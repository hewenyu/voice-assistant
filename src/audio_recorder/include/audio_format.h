#pragma once

namespace voice {

// 音频格式定义
struct AudioFormat {
    int sample_rate = 16000;      // 采样率
    int channels = 1;             // 通道数
    int bits_per_sample = 16;     // 采样位数
    
    AudioFormat() = default;
    AudioFormat(int rate, int ch, int bits) 
        : sample_rate(rate), channels(ch), bits_per_sample(bits) {}
};

// WAV文件头结构
struct WavHeader {
    // RIFF chunk
    char riff_header[4] = {'R', 'I', 'F', 'F'};
    uint32_t wav_size = 0;        // 将在写入时更新
    char wave_header[4] = {'W', 'A', 'V', 'E'};
    
    // Format chunk
    char fmt_header[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_chunk_size = 16;
    uint16_t audio_format = 1;    // PCM = 1
    uint16_t num_channels = 1;    // 默认单声道
    uint32_t sample_rate = 16000;
    uint32_t byte_rate = 0;       // 将计算
    uint16_t sample_alignment = 0; // 将计算
    uint16_t bit_depth = 16;      // 默认16位
    
    // Data chunk
    char data_header[4] = {'d', 'a', 't', 'a'};
    uint32_t data_bytes = 0;      // 将在写入时更新
    
    WavHeader(const AudioFormat& format) {
        num_channels = format.channels;
        sample_rate = format.sample_rate;
        bit_depth = format.bits_per_sample;
        sample_alignment = num_channels * (bit_depth / 8);
        byte_rate = sample_rate * sample_alignment;
    }
    
    void update_sizes(uint32_t data_size) {
        data_bytes = data_size;
        wav_size = data_size + sizeof(WavHeader) - 8;  // -8 因为RIFF头大小不包含在内
    }
};

} // namespace voice 