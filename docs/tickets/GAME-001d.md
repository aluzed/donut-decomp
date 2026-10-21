# GAME-001d — Extraire GameStateMachine (transitions splash/menu/ingame/pause/result via enum+table)

- **Status:** DONE (2026-10-21)
- **Priority:** P1
- **Module:** Game
- **Depends on:** —
- **Files:** `src/Game.h:15-25` (enum `GameState`), `src/Game.h:137,140` (`_gameState`/`_missionCompleteTimer`), `src/Game.cpp:587-613` (Result/Splash), `src/Game.cpp:749-767` (Pause/quit), `src/Game.cpp:792-798` (court-circuit Paused)

## Contexte
Les transitions d'état sont des `if` dispersés dans `Game::Run` : minuteur Result→InGame
(`Game.cpp:587-604`), Splash→MainMenu (`Game.cpp:606-613`), InGame↔Paused (`Game.cpp:749-755`),
quit depuis pause (`Game.cpp:763-767`) et le court-circuit de rendu en Paused
(`Game.cpp:792-798`). L'enum vit dans `Game.h:15-25`, l'état dans `Game.h:137`.

## Approche
1. Créer `src/Game/GameStateMachine.{h,cpp}` portant `_gameState` (`Game.h:137`) et
   `_missionCompleteTimer` (`Game.h:140`).
2. Définir une table `{from, GameAction trigger, to}` couvrant Splash→MainMenu
   (`Game.cpp:608-610`), InGame→Paused / Paused→InGame (`Game.cpp:749-755`), Paused→quit
   (`Game.cpp:763-767`).
3. Gérer les transitions temporisées hors table : `Update(dt)` qui avance le timer Result et
   déclenche Result→InGame + cleanup mission (`Game.cpp:587-604`).
4. Exposer `GetState()`/`SetState()` (déjà `Game.h:78-79`) en délégant à la SM ; remplacer les
   `if (_gameState==)` dispersés par des appels SM. Aligner / fusionner avec GAME-003 (même table).

## Done (2026-10-21)

`src/Game/GameStateMachine.{h,cpp}` holds the game's state (`GameState`, moved there) and the
result screen's clock. The 23 assignments scattered through `Game.cpp`, `GameSprint.cpp` and
`ScriptEngine.cpp` are events (`Play`, `Pause`, `MissionWon`, `MissionLost`, `ResultOver`,
`OpenShop`, `OpenPhoneBooth`, `LevelOver`, `ShowCredits`, `ToMainMenu`, `PlayMovie`,
`OpenMiniGame`, `ShowSplash`) fired at one table of `{event, states it may come from, state it
goes to}`; one fired from a state the table does not list is refused and logged ("Game:
OpenMiniGame refused in state Splash"), which found the one way the first table missed: the
bonus game started straight from the title (`--minigame`), which then crashed. The result's
five seconds are `UpdateResult`; the movies go back where they came from by `eventTo`.

Checked: `val.sh 240`, the bonus game and a start from the logos and the title, with no
transition refused.

## Critères d'acceptation
- [x] `_gameState` and `_missionCompleteTimer` live in `GameStateMachine`, no longer in `Game`.
- [x] One table drives the states' transitions; `Run` fires events instead of assigning.
- [x] The result screen's timer and the mission's cleanup after it work as before.
- [x] The same states and transitions are reached (none refused over the runs above).
- [x] The game builds and runs as before.
