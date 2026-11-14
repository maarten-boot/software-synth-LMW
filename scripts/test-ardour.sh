#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Spec S9.2, Phase 8c: test the LMW-Synth VST3 in Ardour 9.9 (GPLv2+, built from source in a container;
# user's permission 2026-11-14), headless with Ardour's Lua session tool and its Dummy backend:
# VST3 scan, instrument on a MIDI track, a note gives sound (track meter), silence after the release,
# parameters set by Ardour reach the plugin (read back over 9P), session save / reload restores them,
# plays after the reload. 9P writes, cables, notes and the editor need Ardour's GUI (the plugin's
# message thread runs on its GLib main loop); that part is a manual Xvfb test (Tests.md T-S9.2-ardour).
#
# Usage:  scripts/test-ardour.sh            (uses build-u24/VST3 and build-u24/lmw9p: run build-ubuntu24.sh first)
# The first run builds Ardour (about 1 hour on 4 cores) into the volume lmw-ardour-build and the image
# localhost/lmw-ardour-deps; later runs take about a minute. Remove both with:
#   podman volume rm lmw-ardour-build; podman rmi localhost/lmw-ardour-deps:1
# Needs podman. HTTPS (git clone) goes through the host CA bundle (TLS-inspecting proxies).
set -euo pipefail

BASE="docker.io/library/ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852"
ARDOUR_TAG="9.9"
ARDOUR_COMMIT="9500a47c504f9e2c8e762b55665c68f2630a5f59"   # tag 9.9 (2026-11-09)
DEPS_IMAGE="localhost/lmw-ardour-deps:1"
VOLUME="lmw-ardour-build"
CA="/etc/ssl/certs/ca-certificates.crt"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[[ -d "$root/build-u24/VST3/LMW-Synth.vst3" && -x "$root/build-u24/lmw9p" ]] || { echo "error: run scripts/build-ubuntu24.sh first" >&2; exit 1; }

if ! podman image exists "$DEPS_IMAGE"; then
    echo "== building $DEPS_IMAGE (Ardour build dependencies)"
    podman rm -f lmw-ardour-deps-tmp >/dev/null 2>&1 || true
    podman run --name lmw-ardour-deps-tmp "$BASE" bash -euo pipefail -c '
        export DEBIAN_FRONTEND=noninteractive
        sed -i "s/^Types: deb$/Types: deb deb-src/" /etc/apt/sources.list.d/ubuntu.sources
        apt-get update -qq
        apt-get build-dep -y -qq ardour >/dev/null
        apt-get install -y -qq git python3 ca-certificates libjpeg-dev libwebsockets-dev libcppunit-dev \
            liblilv-dev libsuil-dev lv2-dev libreadline-dev libsamplerate0-dev libxml2-dev libpangomm-1.4-dev \
            libcairomm-1.0-dev libkeyutils-dev libfftw3-dev xvfb xdotool imagemagick gdb >/dev/null
        rm -rf /var/lib/apt/lists/*'
    podman commit -q lmw-ardour-deps-tmp "$DEPS_IMAGE" >/dev/null
    podman rm lmw-ardour-deps-tmp >/dev/null
fi

echo "== Ardour $ARDOUR_TAG (built once, cached in volume $VOLUME)"
podman run --rm -v "$VOLUME":/build -v "$CA":/proxy-ca.crt:ro -e GIT_SSL_CAINFO=/proxy-ca.crt \
    -e TAG="$ARDOUR_TAG" -e COMMIT="$ARDOUR_COMMIT" "$DEPS_IMAGE" bash -euo pipefail -c '
    cd /build
    [ -d ardour ] || git clone -q --branch "$TAG" --depth 1 https://github.com/Ardour/ardour.git
    cd ardour
    test "$(git rev-parse HEAD)" = "$COMMIT" || { echo "error: ardour is not at $COMMIT" >&2; exit 1; }
    if [ ! -x build/luasession/luasession ]; then
        python3 waf configure --no-phone-home --with-backends=dummy,alsa --optimize > /build/configure.log 2>&1 \
            || { tail -15 /build/configure.log; exit 1; }
        python3 waf -j"$(nproc)" > /build/build.log 2>&1 || { grep -m5 error /build/build.log; tail -20 /build/build.log; exit 1; }
    fi
    echo "built"'

echo "== test"
out="$(mktemp)"; trap 'rm -f "$out"' EXIT
podman run --rm -v "$VOLUME":/build -v "$root/build-u24":/lmw:ro -v "$root/scripts":/t:ro "$DEPS_IMAGE" \
    bash -uo pipefail -c '
    cd /build/ardour && python3 waf install > /tmp/install.log 2>&1 || { tail -20 /tmp/install.log; exit 1; }
    mkdir -p ~/.vst3 && cp -r /lmw/VST3/LMW-Synth.vst3 ~/.vst3/
    sleep 1.1   # Ardour trusts a VST3 cache file only when it is strictly newer than the module
    LD_LIBRARY_PATH=/usr/local/lib/ardour9 /usr/local/lib/ardour9/ardour-vst3-scanner -q -f ~/.vst3/LMW-Synth.vst3
    mkdir -p /work
    PATH=/lmw:$PATH timeout 180 ardour9-lua /t/test-ardour.lua' 2>&1 | grep -v "^$" | tee "$out"
grep -q "^ALL PASS" "$out"
