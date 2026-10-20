# Audio engine

This branch contains the C++20 `audio_engine` extracted from `pi-hifi-player`.
It builds the audio library independently of the original player's UI.

The extraction preserves the original decoders, DSD/DFF support, and platform
sinks. It also includes the tested PCM compatibility, EQ headroom, playback
completion, and macOS output routing fixes developed in the Flutter integration.
The initial source is `pi-hifi-player` commit `b6d3c35`; the audio fixes come from
`pi-music-flutter` commit `ca897ff`.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

macOS uses CoreAudio and AudioToolbox. Linux and Windows require SDL2; Linux
uses ALSA when available. Android uses AAudio. Host-specific integration tests
run only where their audio backend is available. The default tests do not play
audible tones; on macOS `build/core_audio_playback_test --with-output` also
exercises live AudioQueue playback with silence.

The library target is `audio_engine`. Parent CMake projects can include it with
`add_subdirectory(audio_engine)` and link against that target.
