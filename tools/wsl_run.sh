#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Run a command in the repo from WSL with the toolchain set up (clang, Emscripten):
#   wsl -d Ubuntu -- bash /mnt/i/FM1/fm1-berlin-main/tools/wsl_run.sh COMMAND...
cd "$(dirname "$0")/.."
[ -f "$HOME/emsdk/emsdk_env.sh" ] && . "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1
export CC="${CC:-clang}"
exec "$@"
