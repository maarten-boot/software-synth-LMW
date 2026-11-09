#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Build LMW-Synth for Ubuntu 24.04 (and newer) inside a container, on any Linux host (Spec S2.1).
# Result: build-u24/Standalone/LMW-Synth, build-u24/VST3/LMW-Synth.vst3 and build-u24/lmw9p
#
# Usage:  scripts/build-ubuntu24.sh [--clean]
#   --clean   drop the cached container build (forces a full rebuild, re-downloads JUCE)
# Needs podman (preferred) or docker. Override with LMW_CONTAINER=docker.
set -euo pipefail

# Ubuntu 24.04 image, pinned by multi-arch index digest (Spec S2.3). Updated 2026-11-07.
IMAGE="docker.io/library/ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852"
CACHE_VOLUME="lmw-synth-u24-build"     # keeps the CMake build (and JUCE download) between runs

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out_dir="$root/build-u24"

engine="${LMW_CONTAINER:-}"
if [[ -z "$engine" ]]; then
    if command -v podman >/dev/null; then engine=podman
    elif command -v docker >/dev/null; then engine=docker
    else echo "error: need podman or docker" >&2; exit 1; fi
fi

if [[ "${1:-}" == "--clean" ]]; then
    "$engine" volume rm -f "$CACHE_VOLUME" >/dev/null 2>&1 || true
    echo "cache cleared"
fi

# The container writes to a local temp dir (container root cannot write to NFS/root-squashed
# project folders); we then copy the result into the project as the current user.
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

"$engine" run --rm \
    -v "$root":/src:ro \
    -v "$CACHE_VOLUME":/build \
    -v "$stage":/out \
    -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -e ENGINE="$engine" \
    "$IMAGE" bash -euo pipefail -c '
        export DEBIAN_FRONTEND=noninteractive
        echo "== installing build dependencies"
        apt-get update -qq
        apt-get install -y -qq cmake ninja-build g++ curl ca-certificates pkg-config \
            libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev \
            libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev libxi-dev libegl-dev >/dev/null
        echo "== configuring"
        cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null
        echo "== building"
        rm -rf /build/LMWSynth_artefacts      # no stale outputs (e.g. after a product rename); relink only
        cmake --build /build
        echo "== testing"
        ctest --test-dir /build --output-on-failure
        cp -r /build/LMWSynth_artefacts/Release/Standalone /build/LMWSynth_artefacts/Release/VST3 /out/
        cp /build/lmw9p /out/
        # Docker runs as real root: hand the files to the calling user. (Rootless podman maps
        # container root to that user already; chown there would map to a sub-UID instead.)
        if [[ "$ENGINE" == docker ]]; then chown -R "$HOST_UID:$HOST_GID" /out; fi
    '

rm -rf "$out_dir"
mkdir -p "$out_dir"
cp -r "$stage"/. "$out_dir"/
echo
echo "Done (runs on Ubuntu 24.04 and newer):"
echo "  standalone: $out_dir/Standalone/LMW-Synth"
echo "  VST3:       $out_dir/VST3/LMW-Synth.vst3   (copy to ~/.vst3/ to use in a host)"
echo "  9P client:  $out_dir/lmw9p   (e.g. $out_dir/lmw9p ls params)"
