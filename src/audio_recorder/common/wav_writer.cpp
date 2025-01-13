#include "wav_writer.h"
#include <iostream>

namespace voice {

WavWriter::WavWriter() : total_bytes_(0) {}

WavWriter::~WavWriter() {
    if (is_open()) {
        close();
    }
}

bool WavWriter::open(const std::string& filename, const AudioFormat& format) {
    if (is_open()) {
        close();
    }

    format_ = format;
    file_.open(filename, std::ios::binary);
    if (!file_.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    write_header();
    return true;
}

void WavWriter::write_header() {
    WavHeader header(format_);
    header_pos_ = file_.tellp();
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
}

void WavWriter::update_header() {
    if (!is_open()) return;

    WavHeader header(format_);
    header.update_sizes(total_bytes_);

    auto current_pos = file_.tellp();
    file_.seekp(header_pos_);
    file_.write(reinterpret_cast<const char*>(&header), sizeof(header));
    file_.seekp(current_pos);
}

bool WavWriter::write(const void* data, size_t size) {
    if (!is_open()) return false;

    file_.write(static_cast<const char*>(data), size);
    total_bytes_ += size;
    return true;
}

void WavWriter::close() {
    if (!is_open()) return;

    update_header();
    file_.close();
    total_bytes_ = 0;
}

} // namespace voice 