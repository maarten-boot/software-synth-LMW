# LMW Synth

A software synth with GUI and VST, using 9P for remote control.

A semi-modular polyphonic software synthesizer (standalone + VST3), C++20 / JUCE 9.
Licence: GNU AGPLv3 (see `LICENSE`). Design documents: `architecture/`.

## Build
```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```
Standalone app: `build/LMWSynth_artefacts/Release/Standalone/`.

A binary only runs on Linux systems at least as new as the one that built it.
To build for **Ubuntu 24.04 and newer** from any Linux host (needs podman or docker):
```sh
scripts/build-ubuntu24.sh          # result in build-u24/Standalone and build-u24/VST3
scripts/build-ubuntu24.sh --clean  # full rebuild
```
