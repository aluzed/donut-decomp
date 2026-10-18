# HIT-AND-RUN — The meter, the police, busted

- **Status:** PARTIAL (2026-10-18)
- **Priority:** P2
- **Module:** Game
- **Depends on:** PHYS-004 (impacts), AI-RACE (driving), LEVELS
- **Files:** `src/Game/HitAndRun.{h,cpp}`, `src/Game.cpp` (`updateHitAndRun`, impacts, HUD,
  draw), `src/AI/PedestrianManager.cpp` (`KnockDown`)

## Context

Every level script sets up the police (`CreateChaseManager("cPolice", ...)`,
`SetHitAndRunDecay(3.0)`), and none of it did anything: the commands were empty, the
level's `leveli.mfk` is not run, and the existing `ChaseManager` (created only by a
mission that asks) slides boxes towards the player. Running people down cost nothing.

## What is done

- **The meter**, 0-100, shown as the HUD's HIT & RUN bar. Running someone down (a car at
  more than 15 km/h within 2.2m of a pedestrian) adds 25 a person; an impact with a
  traffic car within 6m adds 15. It drains at the level's `SetHitAndRunDecay` per second,
  but not while a police car is within 60m.
- **The police.** Full, it sends the level's chase car (`cPolice`; level 7's `cHears`)
  from a road 60m off, driven along the roads to the player like a mission's chasing car,
  its route drawn again as the player moves. It shows on the minimap.
- **Busted:** the police car within 6m (15m of a player on foot, whom it cannot follow off
  the road) while the player stands still, for a second. The fine is 50 coins, or what the
  purse holds, and a mission under way fails ("MISSION_FAILED_HIT_N_RUN").
- **Evaded:** the police car more than 150m behind for 5s, or the meter drained while it
  is out of sight. It drives off (is removed).
- Nothing of it indoors. Rebuilt with each level, and torn down before the physics world.

Checked on sr2's grid with the meter filled by hand and the player waiting at the wheel:
the police car came from 46m, then 19m, and busted him; 60 coins became 10, and the race
failed.

## Not done

- People are taken off the street when hit; there is no knock-down. They do cry out: one of
  the 33 generic passers-by's "hit by a car" lines (`generic_*\p_hitbyc_*.rsd`), at random.
- `DisableHitAndRun()` (the street races' stages) now switches it off for the stage, meter
  and police with it: filled by hand on sr2, the meter was emptied and nobody came; on m2
  the police came. `SetHitNRun`, `ResetHitAndRun` and the meter commands are still empty. The old `ChaseManager` is left as it was.
- No siren, no "BUSTED" screen of its own: the mission-failed screen says it.

A bust (or the police giving up) crashed the game on the next physics step: the police
car was dropped without `Vehicle::DestroyPhysics`, and its raycast vehicle stayed
registered as an action. `~Vehicle` now takes its physics out itself if nobody has; it
showed up in the unattended level 4 run, where the autopilot's teleports left a chase
behind.
