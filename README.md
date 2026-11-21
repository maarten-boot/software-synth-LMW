# LMW-Synth

A software synth with GUI and VST, using 9P for remote control.

A semi-modular polyphonic software synthesizer (standalone + VST3), C++20 / JUCE 9.
Licence: GNU AGPLv3 (see `LICENSE`). Design documents: `architecture/`.

## Download

Releases (GitHub → Releases): LMW-Synth-<version>-setup.exe for Windows, -windows.zip, -macos.tar.gz, -ubuntu-24.04.tar.gz and -fedora-45.tar.gz, with SHA256SUMS.txt. The two newest releases are always kept; older ones are removed after 14 days (their tags stay, so every version can be rebuilt from source).

Every CI run (Actions tab → a green "build" run → Artifacts) offers LMW-Synth-ubuntu-24.04, -fedora-45, -macos and -windows: the standalone app, the LMW-Synth.vst3 plugin and the lmw9p client. Linux/macOS builds are a .tar.gz inside the zip. The builds are not code-signed, so Windows and macOS warn on first start. Install the plugin by copying LMW-Synth.vst3 to ~/.vst3 (Linux) or ~/Library/Audio/Plug-Ins/VST3 (macOS).

Windows: the -windows download also holds LMW-Synth-<version>-setup.exe. It installs the app (Start menu, Apps & features) and the VST3 plugin, either just for you (no administrator rights; the plugin goes to %LOCALAPPDATA%\Programs\Common\VST3, which some DAWs must be told to scan) or for all users (administrator rights; the plugin goes to C:\Program Files\Common Files\VST3, where every DAW looks). Running a newer setup updates LMW-Synth in place; uninstall in Settings → Apps. Patches and presets in Documents stay. The setup is not signed: SmartScreen asks first (More info → Run anyway). Silent: `LMW-Synth-<version>-setup.exe /S [/CURRENTUSER|/ALLUSERS]`.

## Tutorial
New to LMW-Synth? [docs/tutorial/Tutorial.md](docs/tutorial/Tutorial.md): eleven short lessons; in the app, Presets ▸ Tutorial sets each lesson's sound and puts its steps in the Notes tab.

## Prepare
```sh
scripts/prepare.sh            # lists the build dependencies and which are missing
scripts/prepare.sh --install  # installs the missing ones (apt, dnf or brew; asks for sudo)
scripts/prepare.sh --optional # also Xvfb, xdotool, ImageMagick (GUI checks) and podman (container build)
```
Ubuntu/Debian, Fedora and macOS; on Windows it names what to install (Visual Studio 2026 with C++, CMake, Git, NSIS).

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
scripts/validate-vst3.sh           # Steinberg validator + pluginval, built from source (after a build in build/)
```
