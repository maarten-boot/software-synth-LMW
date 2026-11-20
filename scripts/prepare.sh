#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Build dependencies of LMW-Synth (Spec S2.4): lists what is missing, or installs it. The one list of
# packages: CI (.github/workflows/build.yml) and the Ubuntu 24 container build (scripts/build-ubuntu24.sh)
# install through this script too.
#
# Usage: scripts/prepare.sh [--install] [--optional] [--quiet]
#   (none)       list the required packages and which are missing; exit 1 when any is missing
#   --install    install the missing ones (apt on Ubuntu/Debian, dnf on Fedora, brew on macOS;
#                with sudo unless run as root)
#   --optional   also the optional tools: Xvfb, xdotool and ImageMagick (GUI checks on a virtual
#                display), podman (scripts/build-ubuntu24.sh)
#   --quiet      only errors and what gets installed
# Windows: prints what to install (Visual Studio 2026 with C++, CMake, Git; NSIS 3.13 for the installer).
set -euo pipefail

install=0 optional=0 quiet=0
for a in "$@"; do
    case "$a" in
        --install)  install=1 ;;
        --optional) optional=1 ;;
        --quiet)    quiet=1 ;;
        -h|--help)  sed -n '3,14p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $a (see --help)" >&2; exit 2 ;;
    esac
done
say() { [ "$quiet" = 1 ] || echo "$@"; }

# Required: compiler, CMake, Ninja, git, curl, pkg-config; JUCE's Linux libraries (ALSA, FreeType,
# fontconfig, X11 + extensions, OpenGL/EGL).
apt_required=(cmake ninja-build g++ git curl ca-certificates pkg-config
    libasound2-dev libfreetype-dev libfontconfig-dev libx11-dev libxrandr-dev
    libxinerama-dev libxcursor-dev libxext-dev libgl-dev libxi-dev libegl-dev)
apt_optional=(xvfb xdotool imagemagick podman)
dnf_required=(cmake ninja-build gcc-c++ git curl tar gzip pkgconf-pkg-config
    alsa-lib-devel freetype-devel fontconfig-devel libX11-devel libXrandr-devel
    libXinerama-devel libXcursor-devel libXext-devel mesa-libGL-devel libXi-devel mesa-libEGL-devel)
dnf_optional=(xorg-x11-server-Xvfb xdotool ImageMagick podman)
brew_required=(cmake ninja git)
brew_optional=()

os="$(uname -s)"
family=""
case "$os" in
    Linux)
        id="" like=""
        if [ -r /etc/os-release ]; then . /etc/os-release; id="${ID:-}"; like="${ID_LIKE:-}"; fi
        case " $id $like " in
            *" ubuntu "*|*" debian "*) family=apt ;;
            *" fedora "*|*" rhel "*)   family=dnf ;;
        esac ;;
    Darwin) family=brew ;;
    MINGW*|MSYS*|CYGWIN*)
        echo "Windows: install Visual Studio 2026 (Desktop development with C++), CMake and Git;"
        echo "for the installer NSIS 3.13 (choco install nsis --version=3.13.0). JUCE needs nothing else."
        exit 0 ;;
esac
if [ -z "$family" ]; then
    echo "unsupported system ($os${id:+, $id}): install a C++20 compiler, CMake 3.22+, Ninja, git, curl," >&2
    echo "pkg-config and the development files of ALSA, FreeType, fontconfig, X11 (Xrandr, Xinerama," >&2
    echo "Xcursor, Xext, Xi) and OpenGL/EGL" >&2
    exit 1
fi

case "$family" in
    apt)  wanted=("${apt_required[@]}");  [ "$optional" = 0 ] || wanted+=("${apt_optional[@]}") ;;
    dnf)  wanted=("${dnf_required[@]}");  [ "$optional" = 0 ] || wanted+=("${dnf_optional[@]}") ;;
    brew) wanted=("${brew_required[@]}"); [ "$optional" = 0 ] || wanted+=(${brew_optional[@]+"${brew_optional[@]}"}) ;;
esac

installed() {
    case "$family" in
        apt)  dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q 'install ok installed' ;;
        dnf)  rpm -q --whatprovides "$1" >/dev/null 2>&1 ;;
        brew) brew list --formula "$1" >/dev/null 2>&1 ;;
    esac
}

missing=()
for p in "${wanted[@]}"; do
    if installed "$p"; then say "  ok       $p"; else say "  missing  $p"; missing+=("$p"); fi
done
if [ "$family" = brew ] && ! xcode-select -p >/dev/null 2>&1; then
    echo "  missing  Xcode command line tools (xcode-select --install)"; missing+=("(xcode)")
fi

if [ "${#missing[@]}" -eq 0 ]; then say "all ${#wanted[@]} packages present ($family)"; exit 0; fi
if [ "$install" = 0 ]; then
    echo "${#missing[@]} missing; install them with: scripts/prepare.sh --install$([ "$optional" = 1 ] && echo ' --optional')"
    exit 1
fi

sudo=""
if [ "$family" != brew ] && [ "$(id -u)" != 0 ]; then sudo="sudo"; fi
pkgs=()
for p in "${missing[@]}"; do [ "$p" = "(xcode)" ] || pkgs+=("$p"); done
[ "${#pkgs[@]}" -gt 0 ] || { echo "run xcode-select --install, then this script again"; exit 1; }
echo "installing (${family}): ${pkgs[*]}"
case "$family" in
    apt)  $sudo env DEBIAN_FRONTEND=noninteractive apt-get update -qq
          $sudo env DEBIAN_FRONTEND=noninteractive apt-get install -y -qq "${pkgs[@]}" >/dev/null ;;
    dnf)  $sudo dnf install -y -q "${pkgs[@]}" ;;
    brew) brew install "${pkgs[@]}" ;;
esac
say "done"
