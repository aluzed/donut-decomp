# GAME-001c — Extraire GameHud/GameUiRenderer (tout le dessin 2D HUD/menu/texte d'aide)

- **Status:** DONE (2026-10-21)
- **Priority:** P1
- **Module:** Game
- **Depends on:** —
- **Files:** `src/Game.cpp:927-1164` (HUD : fps/coins/pos/hp, mission, véhicule, aide, chase, result), `src/Game.cpp:1096-1188` (menus Pause/MainMenu), `src/Game.cpp:64-86` (helper `ControlLabel`), `src/Game.cpp:1245-1497` (fenêtres imgui debug : à laisser de côté ou regrouper)

## Contexte
Tout le rendu 2D vit en ligne dans `Game::Run` : projection ortho + `SpriteBatch`
(`Game.cpp:927-934`), compteurs coins/pos/HP (`Game.cpp:936-957`), HUD mission/timer/laps
(`Game.cpp:959-1008`), HUD véhicule + texte d'aide (`Game.cpp:1010-1076`), HUD chase/heat
(`Game.cpp:1078-1094`), écrans Pause/Result (`Game.cpp:1096-1163`) et MainMenu
(`Game.cpp:1166-1188`). Le helper `ControlLabel` (`Game.cpp:64-86`) sert les libellés.

## Approche
1. Créer `src/Game/GameHud.{h,cpp}` (ou `GameUiRenderer`) recevant `SpriteBatch&`, la font,
   les dimensions viewport et un snapshot d'état (santé, état véhicule/vitesse, infos mission
   depuis `ScriptEngine`, compteurs collectibles).
2. Déplacer chaque bloc de dessin en méthodes : `DrawCoreHud` (`Game.cpp:936-957`),
   `DrawMissionHud` (`Game.cpp:959-1008`), `DrawVehicleHud` (`Game.cpp:1010-1076`),
   `DrawHelp` (`Game.cpp:1046-1059`), `DrawChaseHud` (`Game.cpp:1078-1094`),
   `DrawResult` (`Game.cpp:1118-1163`), `DrawPauseMenu`/`DrawMainMenu` (`Game.cpp:1096-1188`,
   avec `_pauseMenu`/`_mainMenu`).
3. Déplacer `ControlLabel` (`Game.cpp:64-86`) dans le même module (helper interne).
4. Laisser les fenêtres imgui de debug (`imguiMenuBar`/`debugAboutMenu`/`drawControlsWindow`,
   `Game.cpp:1245-1497`) hors scope ici (non-HUD jeu) ou les regrouper dans un module debug séparé.

## Done (2026-10-21)

The HUD itself had already left (`UI/Hud.*`, `UI/GameHud.*`). What `Game::Run` still drew is
in `UI/ScreenText.{h,cpp}`: `DrawTextMenu` (the plain pause and main menus, laid out, hovered
and drawn), `DrawMissionResult` (the mission complete and failed screens, from a
`MissionResult` the game fills: its title in the game's words, the times, the reason and
hint, the countdown), `DrawHints` (the hints down the side and the frame rate),
`CentredTextX`, and `ControlLabel`. The result screen's lines, which stood at fixed offsets
from the middle, are centred; its words are the language's (UI-LANG). `Game.cpp` lost 145
lines. Checked: `val.sh 240` as before, the result screen in French on screen.

## Critères d'acceptation
- [x] The 2D drawing of the HUD, menus, results and hints is out of `Game::Run`.
- [x] `ControlLabel` is out of `Game.cpp` (`UI/ScreenText`).
- [x] The HUD, pause and main menus, results and hints show as before (the results now centred).
- [x] The game builds and runs as before.
