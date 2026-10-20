# LEVEL-STREAM — Load and unload a level's zones by distance

- **Status:** CLOSED (not needed on PC; 2026-10-20)
- **Priority:** P1
- **Module:** Level
- **Files:** `src/Level.cpp` (`LoadP3D`, `DynaLoadData`, `loadRegion`/`unloadRegion`),
  `src/Game.cpp` (`loadLevel`)

## Context
The game streamed its zones in and out as the player moved (the dynamic-zone
triggers' DynaLoad strings, "l1z3.p3d:l1z4.p3d;..."), to fit a level into the PS2's
32 MB. Here `Game::loadLevel` loads every zone, road and interior file of the level at
once; `Level::DynaLoadData` parses the strings, but nothing calls it from the triggers.

## Measured (2026-10-20)
Each level started with `--autostart --level n --no-movies --quit-after 10`, one after
another, everything loaded: the whole process's peak memory and the loading time (the
run's time less the ten seconds played).

| Level | Files | Peak memory | Loading |
|---|---|---|---|
| 1 | 19 | 349 MB | ~2.5 s |
| 2 | 13 | 299 MB | ~2.4 s |
| 3 | 16 | 282 MB | ~2.4 s |
| 4 | 19 | 358 MB | ~2.5 s |
| 5 | 13 | 286 MB | ~2.3 s |
| 6 | 16 | 305 MB | ~2.5 s |
| 7 | 17 | 346 MB | ~2.5 s |

Drawn with the frustum culling in place, the game runs at 250 to 320 frames a second.

## Decision
Closed: streaming would save nothing measurable on a PC, and it is the riskiest change
left. `LoadP3D` appends what a file holds to level-wide lists (entities, instances,
composite models, billboards, particles, moving volumes, physics bodies, the resource
manager's textures and shaders) that other systems point into (props, breakables,
gags, animated objects); unloading a zone would need every one of them tracked by file
and released without leaving those pointers dangling.

If it is ever wanted, two ways were weighed:
- the game's own streaming: per-file ownership of all the above, the physics bodies
  removed, the triggers' DynaLoad strings driving `loadRegion`/`unloadRegion`;
- drawing by zone only: everything loaded, only the zones the game would have had
  loaded drawn (the per-file `DrawRange`s exist, for the interiors' `SetDrawOnly`).
