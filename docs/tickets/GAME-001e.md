# GAME-001e — `Game` as a thin orchestrator of its subsystems

- **Status:** DONE (2026-10-21)
- **Priority:** P1
- **Module:** Game
- **Depends on:** GAME-001a, GAME-001b, GAME-001c, GAME-001d
- **Files:** `src/Game.{h,cpp}`, `src/GameLevel.cpp`, `src/GameAutopilot.cpp`,
  `src/Game/GagManager.{h,cpp}`

## Context
`Game` had become the God object: `Game::Run` was 1546 lines, the constructor 467,
`Game.cpp` 5863 lines and `Game.h` 190 data members. Input (GAME-001a), the camera
(GAME-001b), the 2D drawing (GAME-001c) and the state machine (GAME-001d) were already out;
`Run` still did everything else inline, and the levels, story, interiors, autopilot and
gags all lived in `Game.cpp`.

## Done
**`Run` is the frame's phases, in order** (104 lines): poll the intents, then
`updateOnFoot`, `updateCheckpoints`, `updateDriving`, `updateCamera`, the physics step,
`updateAudio`, `updateCarImpacts`, `updateMissionResult`, `updateFallOff`, `drawGuides`,
`updateWorld`, `updateGameplay`, the debug windows and `handleIntents`,
`updateCharacters`, `renderScene` (`drawPeople`, `drawVehicles`, `drawEffects`), `drawHud`
(`fillMissionHud`, `fillRadarHud`, `fillHintHud`, `drawResultScreen`), `drawScreens`.
Each phase is the old block, moved as it was: same order, same conditions.

**The constructor is its steps** (33 lines): `parseArguments`, `openWindow`,
`loadSystems`, `loadShaders`, `spawnPlayer`, `buildMenus`, `buildInGameScreens`.

**Out of `Game`:**
- `GagManager` (`src/Game/GagManager.*`): the level's gags (level.mfk's `GagBegin` ...
  `GagEnd`), which is at hand, walked into, and their drawing. What playing one does
  (its sound or movie, coins, the count shown, the save) stays the game's.
- `src/GameLevel.cpp`: loading a level, its story missions and their progress, the
  loading screen, the interiors.
- `src/GameAutopilot.cpp`: the autopilot and its helpers (`clearOfTraffic`,
  `clearSpotNear`).

**Members grouped by what they are for**, 190 -> 142: `LaunchOptions _launch` (the
command line's switches), `HudMessages _messages` (cards, alerts, counts), `CarDamage`,
`SafePlace` (where a player who falls off is put back), `PlayerMoves` (jumps, dash,
footsteps), `MovieQueue`, `MiniGameState`, `DebugWindows`.

`Game.cpp`: 5863 -> about 5000 lines.

## Acceptance criteria
- [x] `Game` no longer holds the input, camera, HUD or state logic (GAME-001a-d).
- [x] `Run` plus the constructor's wiring: under 400 lines (104 + 33).
- [x] Far fewer members in `Game.h` (190 -> 142, the rest in subsystems and groups).
- [x] The game plays the same: the 32 missions of the validation won, the mini-game,
      start-up.
- [x] Builds and runs as before.
