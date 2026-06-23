# GAME-001d — Extraire GameStateMachine (transitions splash/menu/ingame/pause/result via enum+table)

- **Status:** TODO
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

## Critères d'acceptation
- [ ] `_gameState`/`_missionCompleteTimer` vivent dans `GameStateMachine`, plus dans `Game`.
- [ ] Une table enum+transition pilote splash/menu/ingame/pause/result (plus de `if (_gameState==)` dispersés dans `Run`).
- [ ] Le timer Result→InGame avec cleanup mission fonctionne à l'identique.
- [ ] Les mêmes états et transitions qu'avant sont atteignables (rien régressé).
- [ ] Le jeu compile et reste runnable à l'identique.
