#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The CPU baseline: each tests/listen/baseline_*.amb three times; prints "script run1 run2 run3 median peak"
# (render % of real time, host CPU). Build the host simulator first (host/build_host.sh).
cd "$(dirname "$0")/../.."
mkdir -p build/listen/baseline
for s in tests/listen/baseline_*.amb; do
    n=$(basename "$s" .amb)
    rs=""
    for i in 1 2 3; do
        out=$(build/host/amb_host "$s" build/listen/baseline 2>&1)
        r=$(printf '%s\n' "$out" | sed -n 's/.*render \([0-9.]*\)% of real time.*/\1/p')
        rs="$rs $r"
    done
    peak=$(printf '%s\n' "$out" | sed -n 's/.*ok peak \(-*[0-9.]*\) dBFS.*/\1/p' | tail -1)
    med=$(printf '%s\n' $rs | sort -n | sed -n 2p)
    fail=$(printf '%s\n' "$out" | grep -c FAIL)
    echo "$n$rs  median $med  peak $peak dBFS  fails $fail"
done
