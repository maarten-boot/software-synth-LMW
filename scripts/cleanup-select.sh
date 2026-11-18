#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Spec S9.7: which releases or CI artifacts the cleanup monitor deletes.
# Reads lines "<created, epoch seconds><TAB><id><TAB><name>" (any order) on stdin and prints the lines
# to delete: everything created more than <days> days before <now>, except the <keep> newest.
# Usage: cleanup-select.sh <now epoch> <days> <keep>
set -euo pipefail
now=$1 days=$2 keep=$3
limit=$(( now - days * 86400 ))
sort -t $'\t' -k1,1nr | awk -F '\t' -v keep="$keep" -v limit="$limit" 'NF >= 2 { n++; if (n > keep && $1 + 0 < limit) print }'
