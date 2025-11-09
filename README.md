# mpegeff

mpegeff is a small learning utility for working with media files. It demonstrates basic usage of FFmpeg libraries (libavformat, libavcodec, libavutil, libswresample) to perform two main types of operations:

- Transmux (change container format without re-encoding)
- Transcode (re-encode audio and/or video into other codecs)

This project is intentionally compact and educational: it focuses on clarity and shows how to wire FFmpeg contexts and use basic encoding/transcoding flows rather than being a production-ready tool.

Table of contents
- Features
- Project layout
- Requirements
- Building
- Running the program
- Tests
- Development notes
- Contributing
- License

Features
- Transmuxing: copy streams from one container to another (fast, no re-encode).
- Transcoding: re-encode audio and/or video streams using a chosen codec.
- Command-line interface implemented with `argparse` (via `FetchContent` in CMake).
- Unit tests using GoogleTest (also added via `FetchContent`) — tests live in the repo root `tests/` folder.

Project layout
- `src/` — main project sources
  - `src/algos/` — remuxing/transcoding algorithm implementations
  - `src/types/` — small RAII wrappers for FFmpeg types (codec context, frame, packet, etc.)
  - `src/utils.*` — convenience functions for codec parameter setup
  - `src/main.cpp` — CLI and wiring
- `tests/` — unit tests (placed at project root as requested)
- `CMakeLists.txt` — top-level build configuration
- `README.md` — this file

Requirements
- A C++20-capable compiler (GCC, Clang, MSVC)
- CMake >= 3.10
- pkg-config
- FFmpeg development libraries (headers + libraries): libavcodec, libavformat, libavutil, libswresample
- Network access to fetch third-party CMake projects (argparse, googletest) via `FetchContent` during CMake configure

Example packages to install on common OSes (you may need to adapt names to your distro):
- Ubuntu / Debian:
  - `sudo apt update && sudo apt install build-essential cmake pkg-config libavcodec-dev libavformat-dev libavutil-dev libswresample-dev`
- macOS (Homebrew):
  - `brew install cmake pkg-config ffmpeg`
- Windows:
  - Use MSYS2 or vcpkg to install FFmpeg dev packages, or build FFmpeg yourself. Ensure `pkg-config` works and CMake can find FFmpeg.

Building
1. Create and enter a build directory:
```mpegeff/README.md#L1-4
mkdir -p build
cd build
```

2. Configure with CMake (from `mpegeff/` root):
```mpegeff/README.md#L6-9
cmake ..
```

If pkg-config can't find FFmpeg, ensure your environment has the appropriate `PKG_CONFIG_PATH` set or install the FFmpeg dev packages.

3. Build:
```mpegeff/README.md#L11-13
cmake --build . --config Release
```

After a successful build you'll have:
- `proj` — the application executable
- `unit_tests` — the test binary (if tests were enabled by the top-level CMake; they are enabled by default)

Running the program
The CLI supports transmuxing and transcoding modes. Example usage:

Basic transmux (copy streams, change container):
```mpegeff/README.md#L15-19
./proj -i input.mkv -o output.mp4 --transmux --to mp4
```

Basic transcode (re-encode both audio and video):
```mpegeff/README.md#L21-25
./proj -i input.mkv -o output.mp4 --transcode --codec-audio aac --codec-video libx264
```

Transcode only video and keep audio streams (mux audio as-is):
```mpegeff/README.md#L27-31
./proj -i input.mkv -o output.mp4 --transcode --tm-audio --codec-video libx264
```

Transcode only audio and keep video streams:
```mpegeff/README.md#L33-37
./proj -i input.mkv -o output.mp4 --transcode --tm-video --codec-audio aac
```

Notes about command-line options (implemented in `src/main.cpp`):
- `-i` — input file path
- `-o` — output file path
- `--transmux` — transmux mode (mutually exclusive with `--transcode`)
- `--transcode` — transcode mode (mutually exclusive with `--transmux`)
- `--to` — target container for transmuxing (e.g., `mp4`, `mkv`)
- `--tm-audio` — in transcode mode, don't transcode audio (mux audio instead)
- `--codec-audio` — audio codec to use when transcoding (default `aac`)
- `--tm-video` — in transcode mode, don't transcode video (mux video instead)
- `--codec-video` — video codec to use when transcoding (default `libsvtav1` in the code)

Tests
- Unit tests live in `tests/` in the project root (not under `src/`) and are compiled into the `unit_tests` binary.
- Tests use GoogleTest and are fetched automatically by CMake via `FetchContent`.
- Running tests:
```mpegeff/README.md#L39-41
ctest --output-on-failure
# or run the binary directly
./unit_tests
```

Testing notes
- Some tests will query installed FFmpeg codecs (e.g., `libopus`, `aac`). If those encoders are missing in the system FFmpeg build, related tests may be skipped — this is expected. The test suite is designed to be tolerant of the environment for learning convenience.
- If you want deterministic test behavior, ensure FFmpeg is built with the codecs you expect, or extend/modify tests to mock or isolate FFmpeg calls.

Development notes
- The project composes a `proj_lib` static library from `src/` code so you can link tests and other tools against it easily.
- The code contains small RAII wrappers around FFmpeg structures (e.g., `CodecContext`, `Frame`, `Packet`) to demonstrate resource management.
- Utilities such as `set_audio_codec_params` are located in `src/utils.*`.
- If you're extending the project, keep logic modular: separate parameter configuration, IO (format/context) handling, and encoding loops into small functions to make unit testing easier.

Security and stability
- This project is intended for learning only. It is not hardened for production workloads.
- It does not aim to handle every unusual input case or provide exhaustive error recovery; focus is on illustrating core FFmpeg integration concepts.

License
- The repository includes a `LICENSE` file. Please follow the terms in it when using or contributing.

Acknowledgements
- FFmpeg project — the APIs used in this project.
- `argparse` library by p-ranav — for command-line parsing (fetched with `FetchContent`).
- GoogleTest — for unit testing (fetched with `FetchContent`).
