#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Spec S9.4 / Tests T-S9.4: run the two third-party VST3 validators on LMW-Synth.vst3, both built
# from source at pinned commits (no prebuilt binaries):
#   - Steinberg VST3 validator (vst3sdk v3.8.1_build_84)
#   - Tracktion pluginval v1.0.4, strictness level 10 (editor tests only when a display exists)
# Runs on Linux, macOS and Windows (Git Bash), locally and in CI. Needs no root: nothing beyond
# what building LMW-Synth itself needs. Linux only:
#   - vst3sdk's CMake also configures its editorhost example, which wants gtk+-3.0 and gtkmm-3.0;
#     only the validator target is built, so placeholder .pc files are enough when they are missing.
#   - pluginval enables LADSPA hosting, which needs ladspa.h; it is switched off when that header
#     is missing (only VST3 is tested here).
#   - pluginval links libstdc++ statically, for distributing it; it only runs where it was built
#     here, so that is switched off (Fedora's static libstdc++ fails to link into its PIE default).
# All platforms: pluginval v1.0.4 uses 100ms literals without including <chrono> (recent MSVC no
# longer gets it through other headers), so the script adds that include to its sources.
# Usage: scripts/validate-vst3.sh [path/to/LMW-Synth.vst3]   (default: build/ output)
# Work dir: $LMW_VALIDATOR_DIR, default /tmp/<project folder>/validators. The built validators are
# copied to <work>/bin; when they are there (e.g. restored from a CI cache) nothing is fetched or
# built. Delete the work dir when done.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
plugin=${1:-$root/build/LMWSynth_artefacts/Release/VST3/LMW-Synth.vst3}
work=${LMW_VALIDATOR_DIR:-/tmp/$(basename "$root")/validators}
command -v cygpath >/dev/null && work=$(cygpath -u "$work")
[ -d "$plugin" ] || { echo "no plugin at $plugin (build first)"; exit 2; }

VST3SDK_TAG=v3.8.1_build_84
VST3SDK_SHA=3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
PLUGINVAL_TAG=v1.0.4
PLUGINVAL_SHA=ed19c2c16b57a6d94db391bea3ef4a80b769d5bf

case "$(uname -s)" in
    Linux*) os=linux; exe= ;;
    Darwin*) os=mac; exe= ;;
    *) os=windows; exe=.exe ;;
esac
bin="$work/bin"
validator="$bin/validator$exe"
if [ "$os" = mac ]; then pluginval="$bin/pluginval.app/Contents/MacOS/pluginval"; else pluginval="$bin/pluginval$exe"; fi
mkdir -p "$bin"

fetch() {  # url tag sha dir
    if [ ! -d "$work/$4" ]; then
        git clone -q --depth 1 --branch "$2" --recurse-submodules --shallow-submodules "$1" "$work/$4"
    fi
    local got
    got=$(git -C "$work/$4" rev-parse HEAD)
    [ "$got" = "$3" ] || { echo "$4: expected commit $3, got $got"; exit 1; }
}

jobs=$( (nproc || sysctl -n hw.ncpu || echo 4) 2>/dev/null | head -1)

# Steinberg validator
if [ ! -x "$validator" ]; then
    fetch https://github.com/steinbergmedia/vst3sdk.git "$VST3SDK_TAG" "$VST3SDK_SHA" vst3sdk
    if [ "$os" = linux ]; then
        for pc in gtk+-3.0 gtkmm-3.0; do
            if ! pkg-config --exists "$pc" 2>/dev/null; then
                mkdir -p "$work/fakepc"
                printf 'Name: %s\nDescription: placeholder, editorhost is not built\nVersion: 3.24.0\nLibs:\nCflags:\n' "$pc" \
                    > "$work/fakepc/$pc.pc"
            fi
        done
        [ -d "$work/fakepc" ] && export PKG_CONFIG_PATH="$work/fakepc${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    fi
    cmake -S "$work/vst3sdk" -B "$work/b-vst3" -DCMAKE_BUILD_TYPE=Release \
        -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_RUN_VST_VALIDATOR=OFF >/dev/null
    cmake --build "$work/b-vst3" --config Release --target validator --parallel "$jobs"
    cp "$(find "$work/b-vst3" -type f -name "validator$exe" -path '*Release*' | head -1)" "$validator"
fi

# pluginval
if [ ! -x "$pluginval" ]; then
    fetch https://github.com/Tracktion/pluginval.git "$PLUGINVAL_TAG" "$PLUGINVAL_SHA" pluginval
    if [ "$os" = linux ]; then
        sed -i 's/-static-libstdc++)/)/' "$work/pluginval/CMakeLists.txt"
        if ! echo '#include <ladspa.h>' | ${CXX:-c++} -E -x c++ - >/dev/null 2>&1; then
            sed -i 's/JUCE_PLUGINHOST_LADSPA=1/JUCE_PLUGINHOST_LADSPA=0/' "$work/pluginval/CMakeLists.txt"
        fi
    fi
    # pluginval v1.0.4 uses 100ms literals without including <chrono>; recent MSVC no longer gets it
    # through other headers. Added to those sources (not by a forced-include flag: Visual Studio
    # applies that to C files too, which breaks JUCE's juceaide).
    for f in Source/MainComponent.cpp Source/Validator.cpp Source/tests/BasicTests.cpp; do
        f="$work/pluginval/$f"
        if ! grep -q '^#include <chrono>' "$f"; then
            { echo '#include <chrono>'; cat "$f"; } > "$f.tmp" && mv "$f.tmp" "$f"
        fi
    done
    cmake -S "$work/pluginval" -B "$work/b-pv" -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$work/b-pv" --config Release --parallel "$jobs"
    out="$work/b-pv/pluginval_artefacts/Release"
    if [ "$os" = mac ]; then cp -R "$out/pluginval.app" "$bin/"; else cp "$out/pluginval$exe" "$pluginval"; fi
fi

status=0
echo "== Steinberg VST3 validator $VST3SDK_TAG"
if "$validator" "$plugin" > "$work/validator.log" 2>&1; then
    grep '^Result' "$work/validator.log"
else
    grep -E '^Result|ERROR' "$work/validator.log" || tail -20 "$work/validator.log"; status=1
fi

echo "== pluginval $PLUGINVAL_TAG, strictness 10"
gui=()
[ "$os" != linux ] || [ -n "${DISPLAY:-}" ] || gui=(--skip-gui-tests)
if "$pluginval" --strictness-level 10 ${gui[@]+"${gui[@]}"} --validate-in-process --validate "$plugin" > "$work/pluginval.log" 2>&1; then
    echo "SUCCESS ($(grep -c 'Starting tests' "$work/pluginval.log") test groups${gui:+, no display: editor tests skipped})"
else
    grep -E 'FAILED|!!!' "$work/pluginval.log" | head -20 || true; tail -20 "$work/pluginval.log"; status=1
fi
echo "logs: $work/validator.log, $work/pluginval.log"
exit $status
