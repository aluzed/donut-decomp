#!/usr/bin/env bash
# Launch the donut game from the project root with the system libstdc++.
#
# Conda's libstdc++ (~/miniconda3/lib) is often too old and makes the binary
# fail at startup with "GLIBCXX_3.4.32 not found". Prepending the system lib
# avoids that. Assets are loaded relative to the working directory, so we cd
# into the project root first.
#
# Usage: ./run.sh [game args...]   e.g.  ./run.sh --log-level debug
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

BIN="$ROOT/build/bin/donut"
if [[ ! -x "$BIN" ]]; then
	echo "error: $BIN not found — build first: cmake --build build -j" >&2
	exit 1
fi

export LD_LIBRARY_PATH="/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$BIN" "$@"
