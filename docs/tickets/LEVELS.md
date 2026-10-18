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
| story | `scripts/level0<n>/level.mfk`, its `AddMission` calls in order, each preceded by its "Sunday drive" `<id>sdi.mfk` (go home, talk to Marge, buy the car the mission needs), played as a mission of its own: level 1 is 16 |
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

**Doors that load an interior.** Level 1's doors are interior-entrance locators (type 7)
naming the place. From level 2 a door is a zone trigger (type 5) whose data loads the
interior, `l2i04.p3d@l2z1.p3d:l2z3.p3d:`: the file is what comes before the `@`. Such a
trigger now takes the player in too, and the interior is known by the trigger's locator,
`moe1`, which is what level 2's scripts give as an `interior` objective's destination.
The file is the one just before the `@`, after any the trigger unloads: level 1's front
door is `l1z7.p3d:l1z2.p3d:l1i02.p3d@`.

A door is gone through by a player on foot standing in it, not only on stepping in: one
who drove up to it is inside its volume by the time they get out, and the entry has gone
by. An `interior` objective whose interior the player is already in (gone in during the
stage before) is met at once. Level 4 m5 waited at the Kwik-E-Mart's door for ever on both
counts.
Level 2 m2 (21 stages) used to wait at Moe's door for ever; it now plays through.

**Level 2's files** brought out two loader crashes, both fixed: an `AnimDynamicPhysics`
with no model (`l2_donotenter_Shape`, `l2_railcross_Shape`), and `DynamicPhysics` objects
naming meshes they do not carry (`l2_lampoldShape*`). Both are now skipped with a warning;
they are missing from the level.

**Levels 3-6** each crashed on load, for four reasons, all fixed:

- level 3's player, Lisa, had no model in `art/chars` -- the local copy of the art was
  incomplete, not the extraction (`cp -a files/extracted/Art/. art/` brings her, and
  every other character, in). A character without a model still falls back to Homer,
  with a warning, rather than crashing;
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
  (45s each): level 3 and level 7 play through stages, level 5 reaches its `dump` stage.
  With the art copied in full, level 3 is played by Lisa in her car, racing the nerd.

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

## Level 2 end to end (2026-10-18, later)

`--level 2 --story --autopilot` plays m1 to m8 and carries on into level 3. What it took,
beyond the above:

- A car put down at a point under a bridge was put on the bridge: the ground search
  started 50m above the point and found the deck first. m6's monkey under the overpass
  was never picked up, and the stage timed out on each retry. The search now looks 3m
  above the point first.
- `--autopilot` caught up with a car it follows only beyond 100m, and moves every 2s: at
  75 km/h the car gains 40m between two looks, past level 4 m1's 120m follow distance.
  It now catches up beyond 60m.
- A missing shader was reported every frame it was asked for; once now.
- "Fallen off the map" was anything below y = -50. Level 3's lower town lies at -68: the
  player sent to talk to Milhouse there was respawned, again and again, and m2 never got
  past him. The line is now 40m below the level's lowest road (-50 at the highest): -120
  on level 3. Its m2, m3 and m4 then play through.

## The whole story (2026-10-18, later)

Run unattended level by level, every level's story plays through: level 1 into level 2,
level 5's m4 to m8 into level 6, level 7's m1 to m8. Winning level 7's last mission ends
the story and returns to the main menu (it replayed that mission for ever). Two more
things on the way:

- A follow or destroy objective decided while its car sat at the end of its route was
  decided again every frame of the results screen: level 5 m4 "completed" 800 times. The
  end of the route is only looked at while the mission is undecided.
- A character file that is a skeleton alone (`npd`, `nps`, `ndr`, the generic drivers)
  crashed the game when a mission placed it (MISSION-STAGES).

The first mission's title card names the level ("LEVEL 2 - Detention Deficit Disorder",
from the phrase table's `LEVEL_2`), and the last one's results say "Level Complete".

## Not done

- Nothing left out any more. The old lamps name their mesh after the instance
  (`l2_lampoldShape3`) while the object carries it as `l2_oldstreetlamp_Shape`: an object
  with a single mesh now draws it whatever the name. The do-not-enter signs and the level
  crossing are back: their model comes in an `AnimDynamicPhysicsWrapper` (0x3F0000F), laid
  out like the `AnimObjectWrapper` level 1 uses, which the parser now reads too (a hand
  edit of `P3D.generated.cpp`: the generator in `dev/codegen` is a Windows binary). Not
  looked at on screen yet.
