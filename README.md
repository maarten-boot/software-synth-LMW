# LMW Synth

A semi-modular polyphonic software synthesizer (standalone + VST3), C++20 / JUCE 8.
Licence: GNU AGPLv3 (see `LICENSE`). Design documents: `architecture/`.

## Build
```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```
Standalone app: `build/LMWSynth_artefacts/Release/Standalone/`.
