#!/bin/bash
# Imports the game's executable and its Direct3D 8 renderer into a Ghidra project,
# analyzes them, and writes their functions, strings (with their users), imports,
# exports, classes, virtual function tables and decompiled C into an output folder.
#
# usage: tools/ghidra/analyze.sh <ghidra install> [output dir]
#   e.g. tools/ghidra/analyze.sh ~/ghidra_12.1.2_PUBLIC
# The binaries come from the game's CDs (files/extracted/Executables, see
# docs/ASSETS.md); the output is the game's code, not ours, and stays out of git
# (build/ghidra by default).
set -e
GHIDRA=${1:?usage: $0 <ghidra install> [output dir]}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${2:-$ROOT/build/ghidra}
BIN=$ROOT/files/extracted/Executables
HERE=$ROOT/tools/ghidra
mkdir -p "$OUT/project"
HEADLESS="$GHIDRA/support/analyzeHeadless"

# Analysis (about 15 minutes for both), then the exports.
"$HEADLESS" "$OUT/project" shar -import "$BIN/pddidx8r.dll" "$BIN/Simpsons.exe" -max-cpu 8 \
	-scriptPath "$HERE" -postScript ExportAll.java "$OUT" > "$OUT/analyze.log" 2>&1
# Decompiled C of every function (about 20 minutes for the executable).
"$HEADLESS" "$OUT/project" shar -process -noanalysis \
	-scriptPath "$HERE" -postScript DecompileAll.java "$OUT" > "$OUT/decompile.log" 2>&1
# The virtual function tables, from the RTTI (Ghidra labels the type
# descriptors but not the tables).
python3 -I "$HERE/rtti_vtables.py" "$BIN/Simpsons.exe" > "$OUT/Simpsons.exe.vtables.tsv"
ls -l "$OUT"
