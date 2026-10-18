# MISSION-STAGES — Missions are sequences of stages

- **Status:** PARTIAL (2026-10-18)
- **Priority:** P1
- **Module:** Scripting
- **Depends on:** MISSION-LOOP, SCRIPT-D
- **Files:** `src/Scripting/ScriptEngine.cpp` (`RunFile`, `beginStage`, `objectiveComplete`,
  `updateObjective`), `src/Scripting/GameCommands.cpp`, `src/Game.cpp` (checkpoints on foot,
  `--autopilot`), `src/ResourceManager.cpp`

## Context

A game mission script declares its stages one after the other (`AddStage` ... `CloseStage`), and
each stage has one objective. The engine ran every command as it was read, so the stages all
applied at once and **the last one declared won**: sr2's post-race dialogue reset the race to
two laps and cleared its checkpoints. The first stage was the only one that could ever be
played, and even that was half overwritten.

Two commands made it worse:

- `ShowStageComplete()` completed **the whole mission** the moment it was read. In the game it
  only asks for the "stage complete" announcement when the stage ends. The real m1 calls it in
  its first stage.
- `AddStage("final")` went through `std::stoi`, which throws.

## What is done

- **Recording.** `ScriptEngine::RunFile` routes each statement. A stage's statements, from
  `AddStage` to `CloseStage`, are kept; everything else runs as it is read. `CloseMission`
  starts stage 1.
- **`beginStage`** clears what is stage-scoped (objective, checkpoints, laps, race drivers,
  waypoints, countdown, stage clock, traffic) and runs the stage's own statements, so every
  existing command works unchanged. Cars stay in the world across stages: m1 parks Skinner's
  car in one stage and races it in the next (`ActivateVehicle`).
- **Objectives** complete as follows:

  | objective | met when |
  |---|---|
  | `goto` | the player reaches `SetDestination`'s locator, as a one-checkpoint lap |
  | `interior` | the player goes in through the door, on foot (GAME-014); the entrance is the HUD marker. A door with no interior behind it completes on arrival |
  | `race` | the player completes the laps (an opponent finishing first fails the mission) |
  | `getin` | the player is in a car |
  | `talkto` | the player comes within 4m of `SetTalkToTarget`'s NPC, where `AddNPC` put them |
  | `dialogue` | 3s pass. There is no dialogue system |
  | `timer` | `SetDurationTime` seconds pass |
  | `delivery` | every `AddCollectible` item has been picked up (within 4m), in any order; the HUD shows "delivery 3/9" |
  | `follow` | the target car, driven along the stage waypoints like a one-lap race, reaches the end. The `followdistance` condition (`SetFollowDistances`) fails the stage if the player falls further behind than its maximum -- once the player has had the car within it: level 5 m4's Wiggum sets off 300m from the player, and the stage failed as it began. A car that reaches its end without ever having been caught up with fails it too |
  | `destroy` | the target car, fleeing along the stage waypoints, is rammed to 0 health. There is no car damage model, so a hit is judged from the gap: touching (under 5m) while closing faster than 4 m/s costs 3 points per m/s. The target reaching the end of its route has got away, and fails the stage |
  | `dump` | every item has been knocked out of the target vehicle (one per ram, dropped where it is) and picked up. The items' locators are placeholders 110m underground, so they are only names |
  | `losetail` | the car on the player's tail is `SetObjDistance` metres behind (150 by default). A car activated as `chase` is driven along the roads to the player, its route drawn again as the player moves on; one more than 80m off when activated is first brought to the road 40m behind the player, the game's own chase script not being run. `--autopilot` makes for a road beyond that distance, straight away from it |
  | `gooutside` | the player comes back out of the interior |
  | `coins` | the purse holds `SetCoinFee`'s fee, which is taken (GAME-010) |
| `fmv`, `buycar` | at once: there is no movie or showroom to hold the player |
  | anything else | at once, with a warning, so it does not block the mission |

  When the last stage's objective is met, the mission is complete.
- Checkpoints, and so `goto`, are reached **on foot** as well as by car.
- `ShowStageComplete()` only flags the announcement. `AddStage` takes non-numeric labels.
- **`--autopilot`** moves the player onto each objective in turn, two seconds apart, and into a
  car for a `getin`, and out of it for an `interior` or a `talkto`. It exists to run a whole
mission unattended.

## A crash it uncovered

On the second run of m1, the game crashed in the sky's `Texture::Bind`. `ResourceManager`
**freed** a texture, shader or mesh when one of the same name was loaded again (a car reloaded
on a restart, two cars sharing `Windsheild.bmp`), while shaders hold raw pointers to their
textures and meshes to their shaders. The replaced resource is now retired and kept alive, and
the latest load still wins the name.

## Verified

`--mission files/extracted/Scripts/missions/level01/m1i.mfk --autopilot`: all six stages in order
(goto the tomato, goto Skinner, race `skinn_v` to the school, the school's entrance, talk to
Lisa, the dialogue), mission complete, restart. Four full cycles in 80s, no crash.

## Every level 1 mission plays through (2026-10-18, later)

Run unattended with `--autopilot`, 60-70s each, all level 1 missions but the demo now reach their
end and restart. Here is the last stage met, and what was skipped on the way:

| mission | stages | reaches | skipped |
|---|---|---|---|
| m0 | 6 | end, 5 cycles | `talkto` Apu (he is in the Kwik-E-Mart, an interior) |
| m1 | 6 | end, 4 cycles | — |
| m2 | 12 | end, 2 cycles | — |
| m3 | 4 | end | — (`smith_v` rammed to destruction) |
| m4 | 4 | end, 6 cycles | — (nine power boxes picked up) |
| m5 | 6 | end | — (the van is followed for 181s) |
| m6 | 4 | end | — (ten games knocked out of the truck and picked up, 79s) |
| m7 | 3 | end, 3 cycles | — |
| bm1 | 7 | end, 5 cycles | — (5 boxes, then 12 Tomacco) |
| sr2 | 2 | end | — |
| gr1 | 2 | end (after the fixes below) | `coins` fee waived for `--autopilot` |
| d1 | 1 | — | its locators are not loaded |

What it took beyond the sequencer:

- **The player's own car.** `SetMissionResetPlayerOutCar(player, car)` parks the family sedan
  at `car`, and `SetMissionResetPlayerInCar(car)` puts the player at its wheel there. m0, m2,
  m3 and bm1 all open on "getin" that car, which did not exist. `PlacePlayerCar` moves the car
  the player already has instead of building a second one.
- **Conditions.** `AddCondition` / `SetCondTime` / `SetCondMinHealth`: "outofvehicle" fails
  the stage once the player has been out of their car longer than the given time, and
  "damage" fails it once the car's health meter reaches the given fraction. "timeout" is the
  stage clock, and "race" / "position" are the race result.
- A car teleported onto the spot where it stands looked for the ground with a ray that hit
  its own roof, and was dropped from 1.3m up. Vehicle ground searches now see only static
  geometry.

## gr1, the race against the clock

gr1 failed the instant its race began. It sets `SetStageTime(1)` and then `UseElapsedTime()`, a
clock that counts **up**, with `SetParTime(195)` as the time not to exceed. The clock counted
down from 1s instead. `UseElapsedTime` and `SetParTime` now do what they say: the stage clock
counts up, and running over par fails the stage.

Its first stage, `coins`, charges a 20-coin entry fee (`SetCoinFee`). It is paid from the
player's coin purse (GAME-010) once that holds enough; `--autopilot` is let in for free.

The completion time was "initial stage time minus remaining", which means nothing for a clock
counting up or a stage with none: gr1 finished in **-8.6s**, a new best time, saved to
`donut_save.dat`. The mission's time is now measured from its first stage, and `--autopilot`
runs set no records (teleporting through a mission is not a record).

## The story, end to end

With level 1's mission scripts copied to `scripts/level01` (docs/ASSETS.md), "New Game" starts
m0, and each mission won leads on to the next, through m7. `--story` starts there without
the menu. `--autostart --autopilot --story`: m0 to m7 in a single 200s session, no crash.
Without the scripts, "New Game" plays the test mission as before.

Each story mission won is recorded in `donut_progress.dat` (gitignored, separate from the
best-time save), and the main menu then offers **Continue** from the next one. `--autopilot`
runs record nothing.

## Retrying from RESET_TO_HERE

`RESET_TO_HERE` marks its stage as the retry point. After a failure, `Game` asks
`ScriptEngine::RetryFromCheckpoint` instead of reloading the mission: the player and their car
go back to where the mission's reset commands put them, and the marked stage is played again.
A stage played again declares its cars again, so `spawnVehicle` moves a car that already
exists rather than building a second one. gr1, failing on its clock, retried 7 times in 70s
on a single load with a single car; M1race picks up its race again after the timer.
A completed mission still reloads from the top.

## Not done

- Dialogue. NPCs stand where placed, turning to face a player on foot within 10m; one
  without animations borrows Apu's idle. (Lisa and the rest of the cast were thought not
  extracted: they were, the local `art/` copy was incomplete.)
- A target "getting away" in `destroy` has not been exercised:
  it goes through the same path as a `follow` target arriving, which has.

## A best time per mission (2026-10-18)

There was one best time for the whole game: every mission, on finishing, was measured
against the fastest of all of them, and gr1's bogus -8.6s beat them all. `donut_save.dat`
now holds a line per mission, `L<level>:<mission> <seconds>`; a mission is compared with
its own, and only a positive time can set one. The old file (a bare number, then a
position) has no keys and reads as no records at all.

## A character with no body (2026-10-18)

With the art copied in full, level 1 m5's `AddNPC("npd", ...)` found `art/chars/npd_m.p3d`
and the game crashed the moment it drew it: `npd`, `nps` and `ndr` are the generic
drivers, a skeleton alone with no PolySkin, and a skin model without one dereferenced a
null vertex binding. Such a character is now not placed (with a warning), and a skin model
with nothing loaded draws nothing.

## The level's own people (2026-10-18)

Each `leveli.mfk` stands people about the level: `AddAmbientCharacter("ralph",
"ambient_ralph", 1.3)`, Apu behind the Kwik-E-Mart counter, the Moleman, Patty, Willie...
They are now placed with the level (9 on level 1, 6 on level 2, 4 on level 4, 6 on level
7), idling and turning to face a player who walks up, like a mission's NPCs; one the
mission has placed as its own is not shown twice. Seen: Ralph by the house next to the
Simpsons'. There is still no talking to them.
