#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Spec S7.12 (Phase 31): takes the pictures of the README and the tutorial, each with its lesson's sound and
# the step it shows, so they can be redone after GUI changes. Linux; needs Xvfb and ImageMagick (convert).
# Starts the built standalone on its own X display with its own HOME, drives it over 9P (lmw9p: /preset,
# /params, /cables, /view) and reads each picture from /snapshot (the window as PNG, without its title bar);
# panel pictures are cropped to the panels /view reports.
# Usage: scripts/pictures.sh [build dir (default build)] [output dir (default docs/images)]
# Environment: LMW_PICTURES_DISPLAY (default :97, must be free). Work files in /tmp/<project folder>/pictures,
# removed at the end.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-$root/build}
out=${2:-$root/docs/images}
display=${LMW_PICTURES_DISPLAY:-:97}
app=$build/LMWSynth_artefacts/Release/Standalone/LMW-Synth
lmw9p=$build/lmw9p
work=/tmp/$(basename "$root")/pictures
for tool in Xvfb convert; do command -v "$tool" >/dev/null || { echo "pictures.sh: needs $tool"; exit 2; }; done
[ -x "$app" ] && [ -x "$lmw9p" ] || { echo "pictures.sh: build first (no $app or $lmw9p)"; exit 2; }
mkdir -p "$out"
rm -rf "$work"
mkdir -p "$work/home"

xvfb=""; synth=""
cleanup() {
    if [ -n "$synth" ]; then kill "$synth" 2>/dev/null || true; wait "$synth" 2>/dev/null || true; fi   # it saves its settings on the way out
    if [ -n "$xvfb" ]; then kill "$xvfb" 2>/dev/null || true; wait "$xvfb" 2>/dev/null || true; fi
    rm -rf "$work"
    rmdir "$(dirname "$work")" 2>/dev/null || true
}
trap cleanup EXIT

Xvfb "$display" -screen 0 1920x1080x24 >/dev/null 2>&1 &   # the smallest supported screen (user 2026-11-26)
xvfb=$!
sleep 1
kill -0 "$xvfb" 2>/dev/null || { xvfb=""; echo "pictures.sh: Xvfb could not start on $display (in use? set LMW_PICTURES_DISPLAY)"; exit 1; }
DISPLAY=$display HOME=$work/home "$app" >/dev/null 2>&1 &
synth=$!

# The synth's own 9P socket (Spec S10.13: <pid>-<n>.sock), so another running LMW-Synth is never touched.
sockets=${XDG_RUNTIME_DIR:+$XDG_RUNTIME_DIR/lmw-synth}
sockets=${sockets:-/tmp/lmw-synth-$(id -u)}
sock=""
for _ in $(seq 1 100); do
    sock=$(ls "$sockets"/"$synth"-*.sock 2>/dev/null | head -1 || true)
    [ -n "$sock" ] && break
    sleep 0.2
done
[ -n "$sock" ] || { echo "pictures.sh: the synth did not start (no 9P socket in $sockets)"; exit 1; }
p9() { "$lmw9p" "unix!$sock" "$@"; }

p9 write view "size 1920 1000"                         # the whole default rack: no scroll bars (S6.5, 51b)
p9 write view "keyboard on"
p9 write view "scales on"                               # Spec S6.20: the beginner aids shown
p9 write view "rings on"
p9 write view "scroll 0 0"                             # Spec S6.21: the rack from its top-left

# shot <file> [panel title or area name prefixes...]: the window, or the smallest area holding those panels / areas (+ 8 px; $below px more
# below them, for cables that sag under a row of Schematic blocks).
shot() {
    local file=$1; shift
    p9 write view "pointer away"                         # no hover marks or tooltip
    p9 write view "focus none"                           # no keyboard cursor
    sleep 0.8                                            # the editor's timer refreshes labels; a tooltip goes
    p9 cat snapshot > "$work/full.png"
    if [ $# -eq 0 ]; then
        cp "$work/full.png" "$out/$file"
    else
        # Panels right of the view (a wide default row, S6.5): scroll the rack until they fit, then shoot again.
        local fit
        fit=$(p9 cat view | awk -v want="$(printf '%s\n' "$@")" '
            BEGIN { n = split(want, w, "\n") }
            $1 == "size" { width = $2 }
            $1 == "scroll" { sx = $2; sy = $3 }
            $1 == "panel" || $1 == "area" {
                title = $6; for (i = 7; i <= NF; ++i) title = title " " $i
                for (k = 1; k <= n; ++k) if (w[k] != "" && index(title, w[k]) == 1 && $2 + $4 > x1) x1 = $2 + $4
            }
            END { if (x1 + 16 > width) printf "%d %d", sx + x1 + 16 - width, sy }')
        if [ -n "$fit" ]; then
            p9 write view "scroll $fit"
            p9 write view "pointer away"
            sleep 0.5
            p9 cat snapshot > "$work/full.png"
        fi
        local box
        box=$(p9 cat view | awk -v below="${below:-0}" -v want="$(printf '%s\n' "$@")" '
            BEGIN { n = split(want, w, "\n") }
            $1 == "panel" || $1 == "area" {
                title = $6; for (i = 7; i <= NF; ++i) title = title " " $i
                for (k = 1; k <= n; ++k) if (w[k] != "" && index(title, w[k]) == 1) {
                    if (!seen || $2 < x0) x0 = $2; if (!seen || $3 < y0) y0 = $3
                    if (!seen || $2 + $4 > x1) x1 = $2 + $4; if (!seen || $3 + $5 > y1) y1 = $3 + $5
                    seen = 1
                }
            }
            END { if (seen) printf "%dx%d+%d+%d", x1 - x0 + 16, y1 - y0 + 16 + below, (x0 > 8 ? x0 - 8 : 0), (y0 > 8 ? y0 - 8 : 0) }')
        [ -n "$box" ] || { echo "pictures.sh: no panel $* on this tab"; exit 1; }
        convert "$work/full.png" -crop "$box" +repage "$out/$file"
        [ -z "$fit" ] || p9 write view "scroll 0 0"
    fi
    echo "  $out/$file"
}

echo "README (String section)"
p9 write preset "String section"
p9 write view "tab Synth";     shot synth.png
p9 write view "tab Effects";   shot effects.png COMB ENSEMBLE DELAY REVERB MAIN SYNTH fxmatrix fxschematic
p9 write view "tab Matrix";    shot matrix.png
p9 write view "tab Schematic"; shot schematic.png

echo "Lesson 1: first sound"
p9 write preset "Tutorial 1"
p9 write view "tab Synth";     shot lesson1-synth.png
p9 write view "tab Schematic"; shot lesson1-schematic.png

echo "Lesson 3: the filter (Cutoff 1 kHz, Reso 20 %)"
p9 write preset "Tutorial 3"
p9 write view "tab Synth";     shot lesson3-vcf.png "VCF 1"

echo "Lesson 4: envelopes"
p9 write preset "Tutorial 4"
shot lesson4-env.png "ENV 1" "ENV 2"

echo "Lesson 5: vibrato (FM 0.3 st, Delay 1 s) and tremolo (50 %)"
p9 write preset "Tutorial 5"
p9 write params/vco1/fm 0.3
p9 write params/lfo1/delay 1
p9 write params/vca/tremolo 0.5
shot lesson5-lfo1.png "LFO 1"
shot lesson5-vca.png "VCA"

echo "Lesson 6: cables (LFO 2 -> VCF 1 Cut at 30 %)"
p9 write preset "Tutorial 6"
p9 write cables "vcf1.cutoff=lfo2.out@0.3"
p9 write view "tab Matrix";    shot lesson6-matrix.png

echo "Lesson 7: FORMANT"
p9 write preset "Tutorial 7"
p9 write view "tab Synth";     shot lesson7-formant.png FORMANT

echo "Lesson 8: FM (VCO 2 -> VCO 1 PM)"
p9 write preset "Tutorial 8"
p9 write view "tab Schematic"; shot lesson8-schematic.png

echo "Lesson 9: effects (ENSEMBLE Solina on)"
p9 write preset "Tutorial 9"
p9 write preset "ensemble Solina"
p9 write view "tab Effects";   shot lesson9-effects.png COMB ENSEMBLE DELAY REVERB MAIN SYNTH
echo "Lesson 9: REVERB first, then COMB (the effects order), on the effects Schematic (S6.31)"
p9 write effects "reverb comb ensemble delay"
p9 write params/reverb/mix 0.25
p9 write params/comb/mix 0.3
shot lesson9-fxschematic.png fxschematic
p9 write effects "comb ensemble delay reverb"

echo "Panel gradient (Menu > Theme > Panel gradient, S6.30), Lesson 1"
p9 write preset "String section"
p9 write view "tab Synth"
p9 write view "gradient on";   shot lesson1-gradient.png KEYS "LFO 1" NOISE "VCO 1"
p9 write view "gradient off"
echo "done"
