# Voice Assistant

A Linux-based voice assistant that captures application audio using PulseAudio, performs Voice Activity Detection (VAD), Speech Recognition, and supports real-time translation.

## Features

- Application-specific audio capture using PulseAudio on Linux
- Real-time Voice Activity Detection (VAD)
- Speech recognition using Sherpa-onnx
- Automatic language detection
- Real-time translation support
- Optimized 16kHz audio sampling for speech recognition
- Automatic audio format conversion (stereo to mono, resampling)

## Dependencies

- PulseAudio development library (`libpulse-dev`) - Linux only
- Sherpa-onnx
- C++17 compiler
- CMake 3.10 or higher

## Building

```bash
mkdir -p build
cd build
cmake ..
make
```

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
- Uses PulseAudio's threaded mainloop for efficient audio capture
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