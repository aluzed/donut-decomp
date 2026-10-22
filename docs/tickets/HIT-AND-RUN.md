# HIT-AND-RUN — The meter, the police, busted

- **Status:** DONE (2026-10-21)
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

- Someone hit is knocked flying: thrown the way the car was going and up, turning over,
  and gone after 1.6s (no ragdoll). They cry out: one of the 33 generic passers-by's "hit
  by a car" lines (`generic_*\p_hitbyc_*.rsd`), at random.
- `DisableHitAndRun()` (the street races' stages) now switches it off for the stage, meter
  and police with it: filled by hand on sr2, the meter was emptied and nobody came; on m2
  the police came. `SetHitNRun`, `ResetHitAndRun` and the meter commands are still empty. The old `ChaseManager` is left as it was.
- The siren (the sound script's "siren", looped, louder as the nearest police car comes)
  came after. The police setting off shows "Hit & Run!" for 3s, a bust "BUSTED!" (the
  phrase table's), in red. As the game's own hints say, going indoors and changing clothes
  empty the meter and lose the police.

A bust (or the police giving up) crashed the game on the next physics step: the police
car was dropped without `Vehicle::DestroyPhysics`, and its raycast vehicle stayed
registered as an action. `~Vehicle` now takes its physics out itself if nobody has; it
showed up in the unattended level 4 run, where the autopilot's teleports left a chase
behind.

## Several police cars, their siren (2026-10-21)

**How many.** `leveli.mfk`'s `SetNumChaseCars`: one on levels 1 to 3, two on 4 to 7.
The meter full, the first comes at once, the others four seconds apart (each from a
road in a direction of its own), while the meter still calls for them. The four
seconds are ours: `SetChaseSpawnRate` is only ever commented out in the scripts, and
`SetHitNRun` (the meter filled) is used only by the E3 demo's scripts. They bust the
player as one did (the nearest within reach); they are shaken off when all are far
behind -- but only once they have closed in: a car comes from the nearest road, at
times 200 m off (level 4's start), and gave up after five seconds before it could
arrive. Not come within reach after thirty seconds, they give up. All are drawn, with
their lights, on the radar, and as obstacles to the traffic.

**Their siren**: the police car's horn in the sound script (`cPolice`'s
`SetHornClipName("siren")`: `sound\carsound\siren.rsd`), looped while they chase,
louder as the nearest comes; none indoors, none once they have given up.
