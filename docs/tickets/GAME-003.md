# GAME-003 — Machine à états de jeu complète (Splash→MainMenu→LevelSelect→InGame→Pause→Result→Credits)

- **Status:** TODO
- **Priority:** P1
- **Module:** Game
- **Depends on:** —
- **Files:** `src/Game.h:15-25` (enum `GameState`), `src/Game.h:137` (`_gameState`), `src/Game.cpp:587-613` (transitions Result/Splash), `src/Game.cpp:749-798` (Pause/quit), `src/Game.cpp:1096-1188` (rendu Pause/MainMenu), `src/Game.cpp:232-257` (`_mainMenu`/`_pauseMenu`)

## Contexte
La machine à états est partielle et éparpillée dans `Game::Run`. L'enum `GameState`
(`Game.h:15-25`) couvre déjà Splash/MainMenu/Loading/InGame/Paused/MissionComplete/MissionFailed/Credits,
mais il n'existe ni `LevelSelect` ni `Credits` rendus, et les transitions sont des `if`
dispersés (`Game.cpp:587`, `606`, `749`, `763`). `_mainMenu` n'est rendu qu'en `MainMenu`
(`Game.cpp:1166`), aucun écran LevelSelect/Credits n'existe.

## Approche
1. Étendre l'enum `GameState` (`Game.h:15-25`) avec `LevelSelect` et `Result` (ou réutiliser
   MissionComplete/MissionFailed comme sous-états de Result).
2. Définir une table de transition `static const struct { GameState from; GameAction trigger; GameState to; }`
   en haut de `Game.cpp` couvrant Splash→MainMenu (`UIConfirm`), MainMenu→LevelSelect,
   LevelSelect→Loading→InGame, InGame↔Paused (`PauseToggle`, déjà `Game.cpp:749-755`),
   InGame→MissionComplete/MissionFailed (déjà `Game.cpp:587`), Result→Credits→MainMenu.
3. Remplacer les `if (_gameState == ...)` épars (`Game.cpp:587-613`, `749-767`) par un appel
   unique `applyTransitions()` consommant les `Input::JustPressed(...)` via la table.
4. Câbler le rendu de chaque état dans la zone 2D existante (`Game.cpp:1096-1188`) : ajouter
   les blocs LevelSelect (liste depuis `locations`, `Game.cpp:344-356`) et Credits.

## Critères d'acceptation
- [ ] L'enum `GameState` contient au moins Splash, MainMenu, LevelSelect, Loading, InGame, Paused, Result (Complete/Failed), Credits.
- [ ] Une table de transition pilote toutes les transitions (plus de `if (_gameState==)` dispersés dans `Run`).
- [ ] Chaque état possède un rendu visible (LevelSelect et Credits inclus).
- [ ] Splash→MainMenu→LevelSelect→InGame→Pause→Result→Credits→MainMenu est parcourable au clavier/manette.
- [ ] Le jeu compile et reste jouable.

## The game's main menu page (2026-10-20)

The main menu is the game's own page now (`scrooby2/frontend.p3d`, `MainMenu.pag`): one
entry at a time between its two arrows, at its place at the top of the screen, in its
font -- "RESUME GAME", "NEW GAME", the levels, "QUIT GAME", the text bible's words.
Left and right (or a click on an arrow) turn it, the confirm key (or a click on it)
takes it; not on the frame the menu opens, which the key leaving the splash screen
would take. The HUD is not drawn under it. Checked: the page drawn, NEW GAME confirmed
starts level 1's first mission.

Not done: the 3D frontend behind it (the Simpsons' house, `3dFE.pag`), the other screens
(options, scrapbook, load game), the screen transitions.
