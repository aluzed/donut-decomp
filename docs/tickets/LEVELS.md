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

## Verified

- Level 1 loads the same 16 files as before, and its story plays as before.
- `--level 2`: 15 files, 117 coins, 7 cards, the DMV and Moe's as interiors, Bart beside
  the Honor Roller in front of Springfield Town Hall; m1's six stages complete under
  `--autopilot`.

## Not done

- Going from one level to the next in a single session: the story ends with the level.
- The skipped do-not-enter signs, level crossing and old lamps.
