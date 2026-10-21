# LEVEL-FOG — Fog per zone from the P3D parameters

- **Status:** CLOSED (2026-10-21): the game has no fog; ours is off
- **Priority:** P2
- **Module:** Level / Render
- **Files:** `src/Level.cpp` (`Level::Draw`), `src/UI/LivingRoom.cpp`,
  `assets/shaders/skin.frag`

## Context
The fog was hard-coded: exponential, density 0.0004/m, sky blue (0.62, 0.78, 1.0), set
by `Level::Draw` on the world and billboard programs. The ticket assumed the level files
carried fog parameters per zone, to be read and blended between zones.

## What the game does
**There is no fog in the data.** Every chunk type in the game's uncompressed P3D files
(179 of them, every file under `art/`) is one the engine knows, and none is a fog. The
only compressed files are level 5's mission files (no world geometry). No file under
`art/` or `scripts/` mentions fog.

**The game turns its fog off and never on.** Read in Ghidra (see TOOL-002, `docs/GHIDRA.md`):

- `pddidx8r.dll`'s Direct3D 8 render context has `EnableFog(bool)` (vtable +0x148,
  `0x10005ce0`: `D3DRS_FOGENABLE`) and `SetFog(colour, start, end)` (+0x150,
  `0x10005d10`: `D3DRS_FOGTABLEMODE` linear, `FOGCOLOR`, `FOGSTART`, `FOGEND`).
- `Simpsons.exe` calls them in one place, Pure3D's `tView` drawing itself (`0x572810`;
  `tView`'s vtable is `0x5f87e4`): `EnableFog(view+0xbc)`, and when on,
  `SetFog(view+0x30, view+0x34, view+0x38)`.
- `tView`'s constructor (`0x572a70`) sets the colour to white, start and end to 0, and the
  flag to off. The render manager (`0x4a80a0`), building each render layer's views and
  cameras, sets the flag to off again. Nothing else in the executable writes the flag:
  every write to offset 0xbc (or word 0x2f) was checked, and the ones setting a 1 are on
  a per-player structure of 500 bytes (`0x42a270`). No function writes it from a
  parameter either: the executable holds no fog setter for a view.

## Done
The fog is off: `fogDensity` 0 for the world and billboards (and the skinned characters'
default). The shaders keep their fog term, unused. The living room no longer puts a fog
back after drawing its scene.

## Acceptance criteria
- [x] No hard-coded fog values: the game's (none) instead.
- [~] Fog per zone, blended between zones: does not apply, the game has none.
