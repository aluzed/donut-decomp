# GAME-003 — Machine à états de jeu complète (Splash→MainMenu→LevelSelect→InGame→Pause→Result→Credits)

- **Status:** PARTIAL (2026-10-20: menus, pause, loading, credits, options and sound)
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
burning down as the level loads.

The level's picture is shown in the place the page leaves blank: `dynaload/images/
loading<n>.p3d`, a sprite of DDS tiles (P3D-017), fitted to the page's box -- level 2's
is the Springfield Shopper, "LOW LEVEL EMPLOYEE HARASSES MILLIONAIRE, HOUNDS RELEASED";
level 3's, "BART SIMPSON DISAPPEARS IN BRIGHT LIGHT". It shows from the game's start too,
over the first level's load.

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

## The living room behind the menu (2026-10-20)

The main menu stands over the Simpsons' living room, the game's 3D frontend (`3dFE.pag`):
`resource/pure3d/camset.p3d`'s 190 meshes, 187 placed by its scene graph, seen through
its camera `FE_Camera` (at 2.9, 2.1, 4.6, looking at the television, 1.07 radians wide,
0.1 to 100 m) -- the pink walls, the couch, the television, the plant, the window onto
the garden, the rug with a wasp camera on it, Krusty's race track (`UI/LivingRoom`).

It drew nothing until the meshes did: not one of its 280 primitive groups has an index
list (2204 such groups in the frontend's files, none in the levels'), and a group
without one was drawn with no indices at all. They are drawn in order now
(`Mesh::CreateMeshBuffers`). On the way: a two-sided mesh's draw turns face culling back
on, which culled the 2D drawn after it; the room's draw puts it back as it was.

The television's screen (`TVscreen.p3d`, a composite of its own) shows its static,
running: the file's texture animation, its eight frames round and round.

Not done: the glows and the sparkles.

## The options and the sound (2026-10-20)

The main menu has an Options entry (the text bible's `OPTIONS`), and so does the pause
screen (greyed until now). Both open the game's options page (`Options.pag`, over the
living room or the paused game): Controller, Sound, View Movies, View Credits, Display,
picked as the pause screen's are (up and down, the mouse; the confirm key or a click
takes one, Escape goes back). Sound and View Credits are done; the others are greyed and
passed over, as is View Credits from the pause screen. The console's display mode the
page carries (interlaced or progressive) is hidden.

The sound page (`Sound.pag`) has the game's four sliders: the music, the effects (and
the place's background), the cars (engines, tyres) and the voices. Up and down pick
one, left and right move it a tenth, a click on its bar puts it there; the speaker (a
note for the music, a microphone for the voices) rides the bar at its value, crossed out
at nothing. Each is drawn at its box's size, not its image's: drawn at the image's, the
bar was a quarter of its width and the knob twice its size, behind it. The volumes are
`AudioManager::SetChannelVolume`, heard at once on what is playing, and kept in
`donut_settings.dat`, read at start and written when the page is left. The surround
setting shows stereo, what OpenAL is given. Checked by screenshots of both pages, the
effects at 0.3 and the voices at nothing.

The display page (`Display.pag`): the resolution, among the screen's sizes from 640 x 480
up (SDL's display modes, each once), and the window or full screen (the text bible's
`WINDOW`, `FULL_SCREEN`), changed with left and right or their arrows, taken by Accept
(`ACCEPT`); the colour depth shows 32-bit, greyed. Kept in `donut_settings.dat` with the
volumes and put back at start. Checked by screenshots: the page, and the game opened at
1024 x 768 and 1280 x 720 from the settings.

The game's pages were drawn at twice their 640 x 480, whatever the window: at another
size they ran off it. They are now as large as the window holds, centred
(`FrontendProject`: the scale and offset of its last draw, for the mouse too). At the
1280 x 960 the game opens at, nothing moves.

Controller opens the keyboard page (`PCKeyboardConfig.pag`): the game's fourteen
commands, two keys each. Moving, sprinting (`Boost`), the action key (`Interact`), driving
(the same actions as moving) and the horn show their keys from `keymap.conf`; first
person, attack, jump (the action key's here), the camera's zoom and resetting the car
are not the engine's, greyed. Up and down pick a row, left and right a column, the
mouse a cell; the confirm key or a click waits for a key ("..."), which takes that
cell's place among the action's keys (the pad's kept), saved to `keymap.conf`; Escape
binds nothing. The page's texts are 15 high in a font twice that: they are shrunk to
their boxes' height (`FrontendProject::SetTextsFitHeight`), on this page only. Checked
by a screenshot; a key captured was not, there being no keyboard to press.

Not done: View Movies (the cutscenes are not played: UI-FMV), the
game settings (`GameSettings.pag`: mouse look and sensitivity).

## The race car on the rug (2026-10-20)

Krusty's race track has its car going round: `racecar.p3d` is not a composite (it was
loaded as one, and nothing of it was seen) but a mesh hung in a scene graph under
`locator1`, under a visibility node (`locator1_SceneGraphVis`) the generated reader
skips; its animation `STRN_locator1` (in the multicontroller `FE_racing`, 132 frames at
30 a second) moves `locator1` round the figure of eight, over the bridge and through
the banked loop. The living room reads the scene graph from its chunks (transforms,
through visibility nodes), builds the animation's tracks by the transforms' names with
`SkinAnimation::FromP3D` (as a skeleton's), and draws each node where its parent and
the animation put it, round and round (`LivingRoom::AnimatedScene`).

On the way: the car's mesh is `f1_polySurfaceShape1`, as is one of `camset.p3d`'s,
and the one loaded last replaced the other in the room's meshes; a file's meshes now go
by a prefix ("racecar/"). Checked by screenshots at 3, 3.5 and 4 s: the car on the right
loop, coming out of it, on the top straight, along the track each time.

## The living room's camera (2026-10-20)

The camera flies about the room as the menus open. `camset.p3d`'s `FE_CameraController`
(1036 frames at 30 a second, 21 animations) moves it by two of them: `STRN_CAM`, where it
is, and `STRN_CAM_INTEREST1`, what it looks at (both world positions, nearly a key a
frame). Sampled every 5 frames and seen frame by frame, the flight is a string of round
trips of 60 frames, each out from a hub to an object and back the same way:

- from the room's view (2.89, 2.13, 4.57), held at 260-321 and 380-460: to the newspaper
  (0-40, one way), the plate of donuts (80-140), the television (140-200), the VCR
  opened (200-260), the scrapbook (321-380), the couch (741-800), the race track (880-910,
  one way);
- from the plate of donuts, the options' hub: to the television's screen (460-520 and
  600-660), the game pad by the television (530-590), Krusty's radio (660-720), the wasp
  camera on the rug (915-975), and from it to the screen (975-1035).

The frames are the game code's, not in the data; which page goes to which object is read
from what each shows. The room opens at frame 290; the options play the way to the
donuts (80-110) and back (110-140); from there the sound page goes to the radio, the
display page to the screen, the keyboard page to the pad, and back
(`Game::updateLivingRoomCamera`, `LivingRoom::Play`). The camera plays the frames at 30
a second, its position and aim interpolated between keys. Checked by screenshots: the
room, the flight half way, the options over the donuts, the sound page over the radio.

The pad (frame 560) comes out black: not looked into.

## Homer (2026-10-20)

Homer is in the room: `homer.p3d` (`FE_Homer`), his four skins (`hom_high_*`) on a
skeleton, posed by `PTRN_Motion_Root` (`FE_HomerController`, 61 frames at 30 a second,
round and round), drawn with the skins' program (`SceneSkins`, as the gags' characters).
He sits on the floor before the television, legs out, arms folded, head back, mouth open:
asleep. `SceneSkins` leaves a file's shaders and textures to its other loaders, and
Homer drew nothing until the room loaded them. Checked by screenshots from the room's
view, the flight to the television, and from beside the television facing him (his
bind pose, standing, drawn too, to tell a posing fault from the pose: the pose is the
file's). Not on the couch: the couch has its own glow, Homer his.
