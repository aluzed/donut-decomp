# LEVELS — Every level, not just level 1

- **Status:** PARTIAL (2026-10-18)
- **Priority:** P1
- **Module:** Game / Level / Scripting
- **Depends on:** MISSION-STAGES
- **Files:** `src/Game.cpp` (`levelWorldFiles`, `levelScriptCalls`, start-up), `src/Level.cpp`
  (`FindInteriorFile`, physics-object robustness), `src/Scripting/ScriptEngine.cpp`

## Context

Everything named level 1: the list of world files, the three interiors and which door
leads to which, the mission art folder, the scripts folder, the story's mission list, the
Warp menu's places, the player (Homer) and the player's car (the family sedan), Homer's
walk and idle animations, and the phrase table's mission titles (`L1`).

## What is done

`--level <n>` plays level n (1 by default). What used to be written out is read:

| what | from |
|---|---|
| world files | `art/l<n>_TERRA.p3d`, every `l<n>z*` and `l<n>r*` (where a road comes in an `a` and a `b` version, only `a`: they are the same stretch, before and after a change), the interiors `l<n>i*` |
| mission art | `art/missions/level0<n>/level.p3d`, and each mission's own `<id>.p3d` |
| story | `scripts/level0<n>/level.mfk`, its `AddMission` calls in order |
| Warp menu | the same script's `AddTeleportDest` calls |
| player and car | `leveli.mfk`'s first `AddCharacter` and `InitLevelPlayerVehicle`: Homer and `famil_v` on level 1, Bart and `honor_v` on level 2; "current" in a mission script is that car |
| walk and idle | the character's own `*_loco_walk` / `*_loco_idle_rest` (`hom_`, `brt_`...) |
| traffic and pedestrians | that level's `leveli.mfk` |
| mission titles | `MISSION_TITLE_L<n>_M<k>`: `mK` is K, the races 8-10, the wager race 11, the bonus mission 12 |

**Interiors.** Which file a door leads into is not in the scripts. Each interior file names
its own locators `int_<tag>_...` (`kwik`, `ele`, `sh` on level 1; `dmv`, `moe` on level 2),
and the door's name contains the tag (`KwikEMart`, `SpringfieldElementary`,
`SimpsonsHouse`). `Level::FindInteriorFile` takes the interior whose tag the door's name
contains, the longest such tag. It replaces a table of level 1's three.

**Level 2's files** brought out two loader crashes, both fixed: an `AnimDynamicPhysics`
with no model (`l2_donotenter_Shape`, `l2_railcross_Shape`), and `DynamicPhysics` objects
naming meshes they do not carry (`l2_lampoldShape*`). Both are now skipped with a warning;
they are missing from the level.

**Levels 3-6** each crashed on load, for four reasons, all fixed:

- level 3's player, Lisa, was never extracted (`art/chars/lisa_m.p3d`): a character
  without a model now falls back to Homer, with a warning;
- levels 4 and 6 draw meshes naming shaders no file loads: such a group is not drawn,
  where a null shader was dereferenced;
- level 5's sky has two props of one name, and the second got the first's mesh after it
  had been moved out: a null mesh. Each prop now builds its own;
- level 5's mission art is P3DZ-compressed, which the loader could not read (P3D-016).

## Verified

- Level 1 loads the same 16 files as before, and its story plays as before.
- `--level 2`: 11 files, 117 coins, 7 cards, the DMV and Moe's as interiors, Bart beside
  the Honor Roller in front of Springfield Town Hall; m1's six stages complete under
  `--autopilot`.
- Levels 3 to 7 load and start their first mission under `--autopilot` without crashing
  (45s each): level 3 (Homer standing in for Lisa) and level 7 play through stages, level 5
  reaches its `dump` stage.

## One level after another (2026-10-18, later)

Winning a level's last story mission leads on to the next level's first, in the same
session. `Game::switchLevel` tears down everything built on the level, the physics world
included (mission cars and drivers, NPCs, pedestrians, the player, then the world), and
`Game::loadLevel` -- the level half of start-up, now a function of its own -- builds the
next one, with its own player. `donut_progress.dat` holds the mission and the level
("3 2": level 2's fourth mission), so Continue starts on the right level; a file written
before holds only the mission, read as level 1's. New Game always starts on level 1.

`--from <k>` starts `--story` at the level's k-th mission, to reach the end of a level
without playing all of it. `--story --from 7 --autopilot`: level 1's m7 won, level 2
loaded, Bart standing outside the Try-N-Save on m1, which plays through, then m2.

## Not done

- The skipped do-not-enter signs, level crossing and old lamps.
