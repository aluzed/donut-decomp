#!/bin/bash
# Regenerates src/P3D/P3D.generated.{h,cpp} from p3d.json and
# src/Scripting/Commands.generated.cpp from cmd.json, as committed.
#
# usage: dev/codegen/generate.sh [--check | --unpatched <dir>]
#   --check      only compares the result with the committed files (exit 1 on a
#                difference) and leaves them alone
#   --unpatched  writes the output before hand-edits.patch into <dir>/src/..., to
#                redo the patch: diff -ru it against the committed files
#
# Needs: the .NET runtime (`dotnet`, 8 or later; DOTNET=<path> to point at one),
# clang-format 18 (CLANG_FORMAT=<path>), patch.
#
# donut-codegen.exe was built for the .NET Framework; it runs on .NET 8 given a
# runtime configuration, written next to a copy of it, and
# System.Security.Permissions.dll (from its NuGet package, 8.0.0, net8.0; MIT). Its output then goes
# through what the committed files went through by hand:
#  1. Unix line ends; glm's types become the engine's (glm::vec3 -> Vector3...)
#     and the includes with them;
#  2. clang-format, with the repository's .clang-format;
#  3. hand-edits.patch: what p3d.json cannot say (a chunk read in two layouts,
#     interleaved arrays, a child read into a field of its parent, comments).
# A change to p3d.json that touches the patched classes may need the patch
# redone (--unpatched).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
DOTNET=${DOTNET:-$(command -v dotnet || echo "$HOME/.dotnet/dotnet")}
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}
CHECK=${1:-}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/bin" "$WORK/out/src/P3D" "$WORK/out/src/Scripting"
cp "$HERE"/donut-codegen.exe "$HERE"/*.dll "$WORK/bin/"
cat > "$WORK/bin/donut-codegen.runtimeconfig.json" <<'EOF'
{ "runtimeOptions": { "tfm": "net8.0", "rollForward": "Major",
  "framework": { "name": "Microsoft.NETCore.App", "version": "8.0.0" } } }
EOF

FILES="src/P3D/P3D.generated.h src/P3D/P3D.generated.cpp src/Scripting/Commands.generated.cpp"
# From inside the work folder: the generator also writes Chunks.md and
# Commands.md, an index of what it generated, where it runs.
(cd "$WORK" && "$DOTNET" "$WORK/bin/donut-codegen.exe" --p3din "$HERE/p3d.json" --p3dout "$WORK/out/src/P3D" \
	--cmdin "$HERE/cmd.json" --cmdout "$WORK/out/src/Scripting" \
	--copyright "Copyright 2019-2020 the donut authors. See AUTHORS.md" > "$WORK/codegen.log" 2>&1) \
	|| { cat "$WORK/codegen.log"; exit 1; }

for name in $FILES; do
	f="$WORK/out/$name"
	sed -i -e 's/\r$//' -e 's/glm::vec2/Vector2/g; s/glm::vec3/Vector3/g; s/glm::vec4/Vector4/g' \
		-e 's/glm::quat/Quaternion/g; s/glm::mat4/Matrix4x4/g' \
		-e 's|#include <glm/gtc/quaternion.hpp>|#include "Core/Math/Quaternion.h"|' \
		-e 's|#include <glm/mat4x4.hpp>|#include "Core/Math/Matrix4x4.h"|' \
		-e 's|#include <glm/vec2.hpp>|#include "Core/Math/Vector2.h"|' \
		-e 's|#include <glm/vec3.hpp>|#include "Core/Math/Vector3.h"|' \
		-e 's|#include <glm/vec4.hpp>|#include "Core/Math/Vector4.h"|' \
		-e 's|#include <P3D/P3DChunk.h>|#include "P3D/P3DChunk.h"|' "$f"
	"$CLANG_FORMAT" --style="file:$ROOT/.clang-format" -i "$f"
done
if [ "$CHECK" = "--unpatched" ]; then
	cp -r "$WORK/out/src" "$2/"
	exit 0
fi
(cd "$WORK/out" && patch -s -p1 < "$HERE/hand-edits.patch")

status=0
for name in $FILES; do
	if [ "$CHECK" = "--check" ]; then
		diff -u "$ROOT/$name" "$WORK/out/$name" || status=1
	else
		cp "$WORK/out/$name" "$ROOT/$name"
	fi
done
[ "$CHECK" = "--check" ] && [ $status = 0 ] && echo "the generated files are as p3d.json and cmd.json make them"
exit $status
