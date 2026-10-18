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
  | `interior` | the same, at the interior's entrance locator (`<name>Entry`). Interiors are not loaded |
  | `race` | the player completes the laps (an opponent finishing first fails the mission) |
  | `getin` | the player is in a car |
  | `talkto` | the player comes within 4m of `SetTalkToTarget`'s NPC, where `AddNPC` put them |
  | `dialogue` | 3s pass. There is no dialogue system |
  | `timer` | `SetDurationTime` seconds pass |
  | anything else | at once, with a warning, so it does not block the mission |

  When the last stage's objective is met, the mission is complete.
- Checkpoints, and so `goto`, are reached **on foot** as well as by car.
- `ShowStageComplete()` only flags the announcement. `AddStage` takes non-numeric labels.
- **`--autopilot`** moves the player onto each objective in turn, two seconds apart, and into a
  car for a `getin`. It exists to run a whole mission unattended.

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

## Not done

- Conditions other than the stage clock (`outofvehicle`, `damage`, `followdistance`, and `race`
  as a condition).
- NPCs as characters in the world (they are positions only), dialogue, interiors.
- `delivery`, `coins`, `destroy`, `dump`, `follow`, `fmv`, `buycar` objectives are skipped.
- Resetting to `RESET_TO_HERE` on failure: a failed mission restarts from the beginning.
