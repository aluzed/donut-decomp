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

## The loading screen (2026-10-20)

A level switch shows the game's loading screen first (`scrooby2/backend.p3d`,
`Loading.pag`): its burst background, the fire at the fuse's end, Scratchy tied to the
dynamite, "LOADING" in its font; one frame drawn before the load holds everything up.
The page's text names its style (`fontB_16`), which the page itself maps to a font file
(`FrontendTextStyleResource`, 0x18104: name, version, `fonts\\font0_16.p3d`, the font's
own name); the frontend registers those. The level's own picture, which the game puts
in at run time, is a blank in the page (eight white pixels): hidden.

The fuse is drawn too, from the fire to the dynamite (a polygon, below). Not done: it
burning down as the level loads; the level pictures.

## The pause screen (2026-10-20)

Paused, the game's own screen shows (`ingame.p3d`: `PauseBgd.pag`, its dark overlay;
`Pause.pag`, "PAUSE MENU"; `PauseMission.pag`, its list): CONTINUE, RESTART MISSION,
CANCEL MISSION, OPTIONS, QUIT GAME. Ours are Continue, Restart and Quit; the other two
are shown greyed and passed over. The entry picked is yellow; up and down, or the mouse
moved over one, pick; the confirm key or a click takes it. The mouse left where it was
when the screen opened picks nothing. The HUD is not drawn under it.

**Polygons** (`FrontendPolygon`, 0x18009) are drawn, as the rectangle their points span
in their first colour: the pause screen's overlay, the loading screen's background and
fuse.

## The credits (2026-10-20)

The story over (level 7's last mission won), the credits roll before the main menu: the
`Credits` state, entered at last. Over the game's background page (`CreditsBgd.pag`, a
black polygon) the text bible's `CREDITS_00` to `CREDITS_31` roll up from the bottom, a
line at each backslash, centred, at 60 pixels a second -- "The Simpsons Hit & Run",
"Vivendi Universal Games", "Producer", "John Melchior"... -- until the last has gone,
or the confirm key skips them. Checked by forcing the state: the roll, black behind.

Not done: the credits' own layout (`ViewCredits.pag`: its lines' groups and styles),
the music under them.

## The living room behind the menu, tried (2026-10-20)

The 3D frontend (`3dFE.pag`) is the Simpsons' living room: `resource/pure3d/camset.p3d`
holds 190 meshes, a scene graph placing 187 of them, and the camera `FE_Camera` (at
2.9, 2.1, 4.6, looking at the television, 1.07 radians wide, 0.1 to 100 m), animated by
`FE_CameraController`; Homer, the TV screen, the race car and the glows are files of
their own. Drawn after the 2D pass with the world program, its own view and projection,
the room stayed black although the television's centre projected well inside the screen
(0.41, -0.09), and as black drawn in the 3D pass, into the scene's frame buffer, in place
of the level -- where the player and the cars drawn after it showed. The meshes' vertex
colours are not black (grey-blue, white, green), and over a grey clear nothing is drawn
at all: no fragment comes out, culling and depth test off or not. Not found why; left
out. Learnt on the way: a two-sided mesh's draw turns
face culling back on, which culls the 2D drawn after it.
