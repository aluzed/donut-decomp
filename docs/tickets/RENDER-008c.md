# RENDER-008c — LOD: swap models by distance

- **Status:** CLOSED (2026-10-21): the game has no levels of detail
- **Priority:** P2
- **Module:** Render

## Context
The ticket assumed the models came in several levels of detail (`_lod0/_lod1` names or
separate groups) for the engine to pick from by distance.

## What the game has
None. No mesh, prop or chunk name in any file under `art/` is a level of detail: the
names with "lod" in them are `explode` effects and the Stonecutters' `lodge`. None of the
179 chunk types the files hold is a LOD chunk. The executable has no class or string about
one either (Ghidra, TOOL-002: 1339 classes). Every model is drawn as it is, at every
distance, as the game does.
