# Audio Recorder

A PulseAudio-based application for recording audio from specific applications with support for voice activity detection (VAD) and speech recognition.

## Features

- Record audio from specific applications using PulseAudio
- Support for both WAV and raw audio output formats
- Real-time voice activity detection (VAD)
- Speech recognition using Sherpa-onnx
- Multiple output modes (file, model, or both)
- 16kHz sampling rate optimization for speech recognition
- Automatic audio format conversion (stereo to mono, resampling)

## Dependencies

- PulseAudio development libraries (`libpulse-dev`)
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
./audio_recorder -l
```

This will display a list of applications currently playing audio, along with their sink input indices.

### Record Audio to File

```bash
./audio_recorder -s <sink_index> -f output.wav
```

- `-s, --source <index>`: Specify the source index to record from
- `-f, --file <path>`: Save audio to the specified file (supports .wav and .raw formats)

### Process Audio Through Speech Recognition

```bash
./audio_recorder -s <sink_index> -m config.yaml
```

- `-m, --model <path>`: Use speech recognition with the specified config file

### Record and Process Simultaneously

```bash
./audio_recorder -s <sink_index> -f output.wav -m config.yaml
```

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
    language: "auto"
    decoding_method: "greedy_search"
    use_itn: true

vad:
  model_path: "models/silero_vad.onnx"
  threshold: 0.5
  min_silence_duration: 0.5
  min_speech_duration: 0.25
  max_speech_duration: 30.0
  window_size: 512
  sample_rate: 16000
  num_threads: 1
  debug: false
```

## Implementation Details

### Audio Capture

- Uses PulseAudio's threaded mainloop for efficient audio capture
- Supports automatic format conversion:
  - Stereo to mono conversion
  - Sample rate conversion to 16kHz
  - 16-bit PCM format

### Voice Activity Detection

- Uses Silero VAD model through Sherpa-onnx
- Configurable parameters for silence/speech duration
- Window-based processing for real-time performance

### Speech Recognition

- Supports multiple model types through Sherpa-onnx
- Real-time processing of detected speech segments
- Language detection and timestamps for recognized text

## Output Format

When using speech recognition, the output includes:
```
Time: 1.234s -- 2.345s
Text: Recognized text content
Language: Detected language (if available)
--------------------------------------------------
```

## Notes

- The WAV output is always in 16-bit PCM format
- Speech recognition requires appropriate model files and configuration
- The application can be stopped using Ctrl+C 