#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run tests/run_tests.sh and pass only if the failing tests are exactly the ones named (the known, recorded
# failures while BERLIN is half built: docs/BASELINE.md, the plan's "expected-red" notes):
#   tools/suite_gate.sh recipe-prints app
cd "$(dirname "$0")/.."
mkdir -p build
sh tests/run_tests.sh > build/suite.log 2>&1
got=$(sed -n 's/^  FAIL \([^ ]*\).*/\1/p' build/suite.log | sort | tr '\n' ' ')
want=$(printf '%s\n' "$@" | sed '/^$/d' | sort | tr '\n' ' ')
echo "failing: ${got:-none}"
echo "allowed: ${want:-none}"
[ "$got" = "$want" ] && echo "gate: ok" && exit 0
echo "gate: FAILED (see build/suite.log)"
exit 1
