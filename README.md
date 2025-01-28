# Voice Assistant

[English](README.md) | [中文](README_zh.md)

A voice assistant that captures application audio, performs Voice Activity Detection (VAD), Speech Recognition, and supports real-time translation. Supports both Windows and Linux platforms.

## Features

- Cross-platform support (Windows and Linux)
- Application-specific audio capture (using PulseAudio on Linux, Windows Audio Session API on Windows)
- Real-time Voice Activity Detection (VAD)
- Speech recognition using Sherpa-onnx
- Automatic language detection
- Real-time translation support
- Optimized 16kHz audio sampling for speech recognition
- Automatic audio format conversion (stereo to mono, resampling)

## Dependencies

### Linux
- PulseAudio development library (`libpulse-dev`)
- Sherpa-onnx
- C++17 compiler
- CMake 3.10 or higher

### Windows
- Windows 10 or higher
- Visual Studio 2019 or higher with C++17 support
- CMake 3.10 or higher
- Sherpa-onnx

## Building

### Linux
```bash
mkdir -p build
cd build
cmake ..
make
```

### Windows
```powershell
mkdir build
cd build
cmake -G "Visual Studio 16 2019" -A x64 ..
cmake --build . --config Release
```

## Model Download

Before running the voice assistant, you need to download the required models:

```bash
# Create model directory
mkdir -p models/whisper

# Download Sense Voice model (choose one of the following)
# 1. Standard model (more accurate, larger file)
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.onnx -O models/model.onnx

# 2. Quantized model (faster, smaller file)
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/model.int8.onnx -O models/model.int8.onnx

# Download Sense Voice tokens file
wget https://huggingface.co/csukuangfj/sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17/resolve/main/tokens.txt -O models/tokens.txt

# Download Whisper model (choose one of the following)
# 1. Tiny model (faster, moderate accuracy)
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-tiny.tar.bz2
tar xvf sherpa-onnx-whisper-tiny.tar.bz2 -C models/whisper/
rm sherpa-onnx-whisper-tiny.tar.bz2

# 2. Base model (more accurate, slower)
wget https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/sherpa-onnx-whisper-base.tar.bz2
tar xvf sherpa-onnx-whisper-base.tar.bz2 -C models/whisper/
rm sherpa-onnx-whisper-base.tar.bz2

# Download VAD model
wget https://github.com/snakers4/silero-vad/raw/master/src/silero_vad/data/silero_vad.onnx -O models/silero_vad.onnx
```

Note: After downloading, make sure to update the model paths in your `config.yaml` accordingly.

## Usage

### List Available Audio Sources

```bash
./voice_assistant -l
```

This will display a list of applications currently playing audio, along with their sink input indices.

### Start Voice Assistant

```bash
./voice_assistant -s <sink-input-index> -m config.yaml
```

Parameters:
- `-s, --source <index>`: Specify the sink input index to capture
- `-m, --model <path>`: Path to the model configuration file
- `-l, --list`: List available audio sources

## Configuration

### Model Configuration (config.yaml)

```yaml
provider: "cpu"
num_threads: 4
debug: false

model:
  type: "sense_voice"
  sense_voice:
    model_path: "models/model.onnx"
    tokens_path: "models/tokens.txt"
    language: "auto"  # Supports "zh", "en", "ja", "ko", "yue", etc.
    decoding_method: "greedy_search"
    use_itn: true

vad:
  model_path: "models/silero_vad.onnx"
  threshold: 0.5  # Voice detection threshold (0.0-1.0)
  min_silence_duration: 0.5  # Minimum silence duration (seconds)
  min_speech_duration: 0.25  # Minimum speech segment duration (seconds)
  max_speech_duration: 30.0  # Maximum speech segment duration (seconds)
  window_size: 512  # Processing window size (samples)
  sample_rate: 16000  # Sample rate (Hz)
  num_threads: 1  # VAD processing threads
  debug: false  # Enable debug output

translator:
  type: "deeplx"  # Translation service type
  target_language: "zh"  # Target language for translation
```

## Output Format

The voice assistant outputs recognition results in the following format:
```
[Recognition Result]
Time: 1.234s -- 2.345s
Text: <recognized text>
Language Code: <detected language>
Target Language: <translation target>
Translated Text: <translated text> (if language differs from target)
--------------------------------------------------
```

## Implementation Details

### Audio Capture
- Linux: Uses PulseAudio's threaded mainloop for efficient audio capture
- Windows: Uses Windows Audio Session API (WASAPI) for low-latency audio capture
- Automatic format conversion:
  - Stereo to mono conversion
  - Sample rate conversion to 16kHz
  - 16-bit PCM format

### Voice Activity Detection
- Uses Silero VAD model through Sherpa-onnx
- Configurable silence/speech duration parameters
- Window-based real-time processing

### Speech Recognition
- Supports multiple model types through Sherpa-onnx
- Real-time processing of detected speech segments
- Language detection and recognition timestamps

### Translation
- Supports real-time translation through DeepLX
- Automatic language detection and translation
- Configurable target language

## Notes

- Speech recognition requires appropriate model files and configuration
- Use Ctrl+C to stop the application
- Ensure target application is playing audio before capturing
- Use `-l` option to check available audio sources before starting
- Translation is performed only when detected language differs from target language 

## Acknowledgments

This project is built upon and inspired by the following excellent projects:

- [sherpa-onnx](https://github.com/k2-fsa/sherpa-onnx): A powerful speech recognition toolkit that provides high-quality speech recognition capabilities
- [obs-studio](https://github.com/obsproject/obs-studio): The audio capture implementation references OBS Studio's PulseAudio capture module

We are grateful to the developers of these projects for their outstanding work. 