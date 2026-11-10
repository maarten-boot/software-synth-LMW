#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Spec S9.4 / Tests T-S9.4: run the two third-party VST3 validators on LMW-Synth.vst3, both built
# from source at pinned commits (no prebuilt binaries):
#   - Steinberg VST3 validator (vst3sdk v3.8.1_build_84)
#   - Tracktion pluginval v1.0.4, strictness level 10 (editor tests when $DISPLAY is set)
# Works without root: nothing beyond what building LMW-Synth itself needs.
#   - vst3sdk's CMake also configures its editorhost example, which wants gtkmm-3.0; only the
#     validator target is built, so a placeholder gtkmm-3.0.pc is enough when it is not installed.
#   - pluginval enables LADSPA hosting, which needs ladspa.h; it is switched off when that header
#     is missing (only VST3 is tested here).
# Usage: scripts/validate-vst3.sh [path/to/LMW-Synth.vst3]   (default: build/ output)
# Work dir: $LMW_VALIDATOR_DIR, default /tmp/<project folder>/validators (sources and builds are
# kept there for reruns; delete it when done).
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
plugin=${1:-$root/build/LMWSynth_artefacts/Release/VST3/LMW-Synth.vst3}
work=${LMW_VALIDATOR_DIR:-/tmp/$(basename "$root")/validators}
[ -d "$plugin" ] || { echo "no plugin at $plugin (build first)"; exit 2; }

VST3SDK_TAG=v3.8.1_build_84
VST3SDK_SHA=3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
PLUGINVAL_TAG=v1.0.4
PLUGINVAL_SHA=ed19c2c16b57a6d94db391bea3ef4a80b769d5bf

mkdir -p "$work"

fetch() {  # url tag sha dir
    if [ ! -d "$work/$4" ]; then
        git clone -q --depth 1 --branch "$2" --recurse-submodules --shallow-submodules "$1" "$work/$4"
    fi
    local got
    got=$(git -C "$work/$4" rev-parse HEAD)
    [ "$got" = "$3" ] || { echo "$4: expected commit $3, got $got"; exit 1; }
}

fetch https://github.com/steinbergmedia/vst3sdk.git "$VST3SDK_TAG" "$VST3SDK_SHA" vst3sdk
fetch https://github.com/Tracktion/pluginval.git "$PLUGINVAL_TAG" "$PLUGINVAL_SHA" pluginval

# Steinberg validator
if ! pkg-config --exists gtkmm-3.0 2>/dev/null; then
    mkdir -p "$work/fakepc"
    printf 'Name: gtkmm-3.0\nDescription: placeholder, editorhost is not built\nVersion: 3.24.0\nLibs:\nCflags:\n' \
        > "$work/fakepc/gtkmm-3.0.pc"
    export PKG_CONFIG_PATH="$work/fakepc${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi
validator="$work/b-vst3/bin/Release/validator"
if [ ! -x "$validator" ]; then
    cmake -S "$work/vst3sdk" -B "$work/b-vst3" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF -DSMTG_RUN_VST_VALIDATOR=OFF >/dev/null
    cmake --build "$work/b-vst3" --target validator
fi

# pluginval
if ! echo '#include <ladspa.h>' | ${CXX:-c++} -E -x c++ - >/dev/null 2>&1; then
    sed -i 's/JUCE_PLUGINHOST_LADSPA=1/JUCE_PLUGINHOST_LADSPA=0/' "$work/pluginval/CMakeLists.txt"
fi
pluginval="$work/b-pv/pluginval_artefacts/Release/pluginval"
if [ ! -x "$pluginval" ]; then
    cmake -S "$work/pluginval" -B "$work/b-pv" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
    cmake --build "$work/b-pv"
fi

status=0
echo "== Steinberg VST3 validator $VST3SDK_TAG"
if "$validator" "$plugin" > "$work/validator.log" 2>&1; then
    grep '^Result' "$work/validator.log"
else
    grep -E '^Result|ERROR' "$work/validator.log"; status=1
fi

echo "== pluginval $PLUGINVAL_TAG, strictness 10"
gui=()
[ -n "${DISPLAY:-}" ] || gui=(--skip-gui-tests)
if "$pluginval" --strictness-level 10 "${gui[@]}" --validate-in-process --validate "$plugin" > "$work/pluginval.log" 2>&1; then
    echo "SUCCESS ($(grep -c 'Starting tests' "$work/pluginval.log") test groups${gui:+, no display: editor tests skipped})"
else
    grep -E 'FAILED|!!!' "$work/pluginval.log" | head -20; status=1
fi
echo "logs: $work/validator.log, $work/pluginval.log"
exit $status
