#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# T-S9.7-cleanup: the selection of scripts/cleanup-select.sh (the monitor's rule, Spec S9.7).
set -u
sel="$(dirname "$0")/../scripts/cleanup-select.sh"
failures=0
check() { if [ "$1" = "$2" ]; then echo "ok   $3"; else echo "FAIL $3: got [$1] expected [$2]"; failures=$((failures + 1)); fi; }
day=86400 now=$((100 * day))
T=$'\t'
# Releases (keep the 2 newest): ages 1, 20, 30, 40 days, given out of order.
releases="$((now - 30 * day))${T}3${T}v1.1.0
$((now - day))${T}5${T}v1.3.0
$((now - 40 * day))${T}2${T}v1.0.0
$((now - 20 * day))${T}4${T}v1.2.0"
check "$(printf '%s\n' "$releases" | "$sel" "$now" 14 2 | cut -f3 | tr '\n' ' ')" "v1.1.0 v1.0.0 " "two newest kept (one is 20 days old), older than 14 days deleted"
# Only 2 releases, both old: none deleted.
check "$(printf '%s\n' "$releases" | head -2 | "$sel" "$now" 14 2)" "" "never fewer than 2 releases"
# 3 releases, the third only 10 days old: kept.
check "$(printf '%s\n' "$now${T}9${T}a" "$((now - 5 * day))${T}8${T}b" "$((now - 10 * day))${T}7${T}c" | "$sel" "$now" 14 2)" "" "younger than 14 days kept"
# Exactly 14 days old: kept (only older ones go).
check "$(printf '%s\n' "$now${T}9${T}a" "$now${T}8${T}b" "$((now - 14 * day))${T}7${T}c" | "$sel" "$now" 14 2)" "" "exactly 14 days kept"
# Artifacts (keep 0): every one older than 14 days.
check "$(printf '%s\n' "$((now - 15 * day))${T}11${T}x" "$((now - 2 * day))${T}12${T}y" | "$sel" "$now" 14 0 | cut -f2)" "11" "artifacts older than 14 days"
check "$(printf '' | "$sel" "$now" 14 2)" "" "nothing in, nothing out"
[ "$failures" -eq 0 ] && echo "test_cleanup: all passed"
exit "$failures"
