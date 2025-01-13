#pragma once

#include <string>
#include <fstream>
#include "audio_format.h"

namespace voice {

class WavWriter {
public:
    WavWriter();
    ~WavWriter();

    // 打开WAV文件准备写入
    bool open(const std::string& filename, const AudioFormat& format);

    // 写入音频数据
    bool write(const void* data, size_t size);

    // 关闭文件并更新头部信息
    void close();

    // 是否已打开
    bool is_open() const { return file_.is_open(); }

private:
    std::ofstream file_;
    std::streampos header_pos_;
    uint32_t total_bytes_;
    AudioFormat format_;
    
    // 写入WAV头
    void write_header();
    // 更新WAV头
    void update_header();
};

} // namespace voice 