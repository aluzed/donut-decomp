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

Not done: the scrapbook's galleries (below).

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

View Movies and the game settings came after (UI-FMV; below).

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

The camera's clipping planes move too: `CAM_FE_Camera`'s `NCLP` and `FCLP` (float
channels, which the generated reader does not keep, read from the chunks) bring the near
plane from 0.1 to 0.01 m, and the far from 100 to 10, for each close look (the newspaper,
the television, the VCR, the pad, the track). Held at 0.1, the pad's close-up (the
camera 10 cm off the floor) cut the floor and the pad away, and came out black. At the
trip's middle (560) the camera is against the pad, the view all purple; the keyboard
page stops three frames before (557), the pad filling the view, and goes back from the
frame as far past it (563). Checked by screenshots at 550, 554, 557 and 560.

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

## The glows (2026-10-20)

The main menu's entry picked has its object in the room glow: `glow<name>.p3d`, a
billboard quad each (an outline of the object, additive) where the file's scene graph
puts it, facing the menu's camera, pulsing yellow to black and back
(`BQG_<quad>`: 23 frames at 30 a second; its colour keys at 0, 14, 15, 16 and 22).
`tv` is the television's outline, `homr` the newspaper's on the floor, `sofa` the
couch's, `vcr` the VCR's, `book` the scrapbook's on top of it, `mini` the race track's.
Resume Game has the television (the camera's trip into it), New Game and the levels the
newspaper (`Newspaper_Spin`), Options the couch (the options' plate of donuts beside it);
the VCR, the scrapbook and the track are the load game, scrapbook and mini game's,
entries this menu does not have. Quit has none, nor the options' pages.

Drawn with the level's billboard program after the room, over it (no depth test: the
television's outline is the television's, Homer in front). That program keeps its own
uniforms, set only when the level draws: `lightmapIntensity` left at 1 turned every
glow black, nothing added. Checked by screenshots of the six, and of the menu's
Resume Game, the television aglow; a screenshot at the black of the pulse shows none.

## The sparkles, looked at (2026-10-20)

`sparkles.p3d` (`FE_Border`, `FE_borderController`: 111 frames at 30 a second) is not
the main menu's: its `Sparklez` transform stands at (-1.01, 1.23, -0.26), just before
the camera's scrapbook station (frames 802-880, the camera at (-0.99, 1.54, -0.26)
looking down at the album). Its `border` quad (`border3_m`, `BQG_borderShape`: dim to
bright and back) is a gilt frame lying on the album's page, seen from there; its
`sparkles` particle system (`sparklesShape22`, `scratch2_red_m`, 200 at most, 50 frames
each) has an emitter whose `TRA` is keyed 21 times over its timeline: it runs round
the frame. Seen drawn at the scrapbook station (the border pulsing; the sparkles a dot
or two, not looked into further), then taken out: there is no scrapbook screen to show
them on.

For when there is: the border is a billboard quad drawn as the glows are; the particle
system needs its emitter's offset taken along its `TRA` keys over the timeline
(`ParticleFactory` reads the first key only, as a fixed offset), which would move any
level emitter keyed the same way.

## The scrapbook (2026-10-20)

The main menu has a Scrapbook entry (the text bible's `SCRAP_BOOK`), the album aglow
(`glowbook`). It flies the camera to the album (321-350; back 350-380) and opens the
game's pages over it, the gilt border and its sparkles about them (`sparkles.p3d`,
above, now shown): `ScrapBook.pag`, Open Book and Game Stats (up and down, the mouse,
the confirm key; Escape back); `ScrapBookContents.pag`, the story's missions, clothes,
vehicles and cards, so many of each; `ScrapBookStats.pag`, the story missions, bonus
missions, street races, cards, clothing, vehicles, wasp cameras, gags, movies and the
game complete (`UI/ScrapBookMenu`).

The counts are the save files': the story's missions won from `donut_progress.dat`
("<mission> <level>", the drives to the missions counted in, as the story plays them)
out of every level's `AddMission` list (54: 8 on levels 1, 2, 5, 6 and 7, 7 on 3 and 4),
the cards, gags and wasp cameras a line each in theirs. What no file keeps (the bonus
missions and street races won, the clothes and vehicles bought: GAME-011), the movies
(not played) and the whole (not reckoned) are greyed, a dash. Checked by screenshots of
the three pages: 46/54 for a save at level 7's first mission.

The sparkles' emitter is keyed along a path (`TRA`, 21 keys): `ParticleFactory` now
keeps every key and emits where the timeline stands (`Emitter::OffsetAt`); of the seven
levels' emitters, logged as each level loaded, none other has such a path. Seen: a
spark or two at a time, not looked into further.

The clothes' gallery (`SkinGallery.pag`) came after, below.

## The cards' gallery (2026-10-20)

Cards, on the scrapbook's contents, opens the cards' gallery (`CardGallery.pag`): a
level's seven, in two rows of four and three, each found card's picture
(`resource/images/cards/db/cardNN.png`) and the card not found yet (`cardred.png`, a
question mark) for the others, the frame of stars (`frame.png`) about the one picked.
Left and right go from card to card, past a level's last to the next level's first; up
and down from row to row; the mouse picks. A card found opens large (`ViewCard.pag` and
`HighResCard.pag`): its picture, title, episode, description and quotes.

The deck is the text bible's: eight a level (`CARD_TITLE_00` "1-1" HOME MADE FOOTBALL
... `CARD_TITLE_07` "1-8" LEVEL 1 BONUS CARD), the seventh found in the level, the
eighth its bonus card, never placed. A card found is kept in `donut_cards.dat` by its
locator's name, `card<level><n>` (`card12`: level 1's second), at (level - 1) * 8 +
n - 1 in the deck. The pages' boxes are the full card's, 191 x 245, the slots 120
apart: the cards are drawn the size of the frame's picture, 84 x 108. The description
and the quotes are wrapped to their boxes, a line their boxes' height
(`FrontendProject::SetTextWrapped`); the text bible's spreadsheet quoting (a phrase in
quotes, each quote doubled) is undone. The small cards and the quotes' speakers on
`ViewCard.pag` are not shown.

Checked by screenshots with a cards file of four (1-1, 1-3, 1-7, 2-1, put in for the
run): the gallery, and the home made football, large, "Episode 8F13: Homer at Bat".
Not shown: which level the gallery is on.

## The missions' gallery (2026-10-20)

Missions, on the scrapbook's contents (the contents' two open entries now picked with
any key or the mouse), opens the missions' gallery (`MissionGallery.pag`): a level's
eight, its m1 to m7 then its bonus mission, in two rows of four, each won its briefing
picture (`misLL_0N.p3d` for mN, `misLL_08.p3d` the bonus mission's, as the scripts'
`SetPresentationBitmap` have them), the locked postcard (`msnlockd.png`) for the others;
the frame of stars about the one picked, its number ("Mission 3", "Bonus Mission") and,
won, its title from the page's own strings (`MISSION_TITLE_L1_M3`, Office Spaced; the
bonus mission's is `_M12`). Keys and the mouse as in the cards' gallery.

A mission is won as the stats count it: before the level reached, or before the
mission reached on it (donut_progress.dat). Level 1's m0, the tutorial, and the levels'
m8 have no place in the gallery; the bonus missions won are not kept (GAME-011), so
theirs stay locked. The pictures are drawn the locked postcard's size, 105 x 83, in the
middle of their boxes (280 square, 120 apart). Checked by screenshots with the save at
level 7's first mission: level 1's seven pictures, its bonus locked, "Mission 3"
"Office Spaced"; level 7's eight locked.

The save now keeps the bonus missions and street races won and the clothes and vehicles
bought (GAME-011): the scrapbook's stats count them, and a level's bonus mission won
shows in the missions' gallery.

## The vehicles' gallery (2026-10-20)

Vehicles, on the scrapbook's contents (now three open entries, left and up going back
through them, right and down on), opens the vehicles' gallery (`VehicleGallery.pag`): a
level's six, `rewards.mfk`'s `BindReward(..., "car", ...)` in its order: the level's own
car, the bonus mission's, the street races', three for sale. Each the player's shows its
picture (`dynaload/images/cars2D/<car>.png`, 270 x 200, as large as its box holds), the
others the page's silhouette (`silhouet/si_car.png`); the glow (`sbcursor.png`) under the
one picked, its name the text bible's (`PLOWK_V`, Plow King), "?" locked. Keys and the
mouse as in the other galleries, two rows of three.

A level's own car is the player's from the start; the bonus mission's with its bonus
mission won, the street races' with its three races won (the save's `won`); those for
sale bought (`car:<level>`). The stats count them out of the 42 (`Vehicles 10/42`).
Checked by screenshots with a save of level 1's bonus mission and three races won and
the Plow King bought: level 1's four (Family Sedan, Pickup Truck, Electaurus, Plow
King), two locked; level 2's own car alone; the stats' 10/42.

## The clothes' gallery (2026-10-20)

Clothes, on the scrapbook's contents (now all four open, round the page clockwise from
the missions on its left), opens the clothes' gallery (`SkinGallery.pag`): a level's
three for sale, `rewards.mfk`'s `BindReward(..., "skin", "forsale", ...)` in its order.
Each bought shows its picture (`dynaload/images/skins2D/<skin>.png`, 119 x 230), the
others the level's character in shadow (the page's own `Skin0` images, a level each:
`silhouet/si_homer.png`, `si_bart`, `si_lisa`, `si_marge`, `si_apu`, `si_bart`,
`si_homer`); the glow (`sbcursor.png`) at the feet of the one picked, its name the text
bible's (`M_PINK`, Marge - Classy), "?" not bought. Left and right go from one to the
next, past a level's last to the next level's first; the mouse picks.

A level's own clothes, worn from the start, have no place in the gallery: the stats
count those bought out of the 21 (`Character Clothing 4/21`), as the save keeps them
(`skin:<level>`). Checked by screenshots with a save of Homer's muumuu, Bart's tall and
ninja and Marge's pink suit bought: each level's three, the stats and the contents.

## The controller's pages and the game settings (2026-10-20)

Controller, on the options, now opens the PC's own page of it (`ControllerPC.pag`):
Character Controls, Vehicle Controls and Game Settings. The first two open the keys'
page (`PCKeyboardConfig.pag`, which has both), at its first row on foot and at its
eleventh in the car; Escape there comes back to it. The camera goes to the game pad as
for the keys before.

Game Settings (`GameSettings.pag`), top to bottom: Use Mouselook Mode (yes or no), Mouse
Sensitivity X and Y (bars, a tenth a step), Invert Mouse X and Y. Left and right change
the one picked, a click on its arrows or along its bar too. Kept in
`donut_settings.dat` (`mouselook`, `mousex`, `mousey`, `invertx`, `inverty`) as soon as
changed. On foot, mouselook off leaves the mouse free and the camera to the keys; the
sensitivity is 0.0005 + 0.009 x the bar a pixel (the half the game's 0.005 of before).
The vehicle's settings, force feedback and the wheel's sensitivity, are greyed: the game
has neither. The page has two sprites named `MouseXSliderIcon` (the mouse's and the
wheel's knobs): the first is renamed (`FrontendProject::RenameSprite`).

The pages' PC strings (`GAME_SETTINGS`, `MOUSE_SENSITIVITY_X` ...) are not in the text
bible's export (`srr2.txt`): they are read from the compiled bible (`srr2.p3d`,
`TextBible::LoadCompiled`) for the terms the export lacks. A language's chunk there
(0x1800E) holds its phrases by a hash of their terms, h = (h * 64 + c) modulo a number
of its own (2990119), sorted, with their offsets into its UTF-16 text; the hash matches
1172 of the 1190 terms whose English phrase is the export's alone.

Checked by screenshots: the controller's page over the pad; the settings with X at 0.8
and Y inverted.

## The phone booth (2026-10-20)

A phone booth, worked by hand, opens the game's car select (`ingame.p3d`'s
`PhoneBooth.pag`) over the game, which stands still (`GameState::PhoneBooth`): every
car the game rewards, one at a time between two arrows, from the one the player drives.
Each its picture (`cars2D/<car>.png`, fitted to the page's box), its name (the text
bible's), and its four stats as five stars each, whole, half or grey (`carstar`,
`halfstar`, `greystar`): rewards.mfk's `SetCarAttributes(car, speed, acceleration,
toughness, stability)`, the order read from the Plow King's 5 for toughness. A car not
had yet is under the page's lock, with its level ("LEVEL 1"). Left and right (or a click
on an arrow) go from car to car, the confirm key (or a click on the picture) calls one
the player has, Escape hangs up.

The car called becomes the player's (`current-car:<level>` in the save, what the
missions' "current" is), brought to the road nearest the booth, repaired, the player at
its wheel (`ScriptEngine::SummonPlayerCar`); the car before is left where it is.

Which cars the player has is one rule now, shared with the scrapbook's vehicles
(`Game::carRewards`): a level's own car always; the bonus mission's with it won; the
street races' with the three won; those for sale bought. Under `--autopilot` the booth
still sends the mission's car straight away.

Not shown: a damaged car's repair cost (`DamagedInfo`; the booth repairs for nothing).
Checked by screenshots with a save of level 1's bonus mission and races won and the Plow
King bought: the Family Sedan's page (1, 1.5, 2.5 and 4 stars), the Duff Truck locked,
"LEVEL 1"; the Plow King called, by the booth, the player at its wheel.

## The shops (2026-10-20)

Cars and clothes for sale can be bought outside the missions now, as in the game.

- The car sellers: leveli.mfk's `AddPurchaseCarReward("gil", "gil", "npd", "gil_loc",
  1.3, "gil_car")`, the shop, who keeps it, where they stand and where a car bought is
  left. They stand among the level's people (Gil; Barney, Homer, Otto... for the
  "simpson" cars). Worked by hand within 2.5 m, they open their shop: this level's cars
  for sale with that shop (rewards.mfk's `BindReward(..., "forsale", <level>, <price>,
  "<shop>")`).
- The wardrobe: an interior's `PurchaseSkin` action trigger (the school's
  `int_ele_skin`, the Kwik-E-Mart's `int_kwik_skin`) opens this level's outfits for sale
  ("interior").

The shop is the game's page (`ingame.p3d`'s `Rewards.pag`, with `Buy.pag` and
`Cancel.pag`) over the game darkened, which stands still (`GameState::Shop`): one item
at a time between two arrows, its name, its price ("125 coins", "to purchase"); a car's
stats as stars in place of its picture with up or down ("View stats", "View model").
Where the game turned a 3D model over its showroom, the item's picture (`cars2D`,
`skins2D`) is shown, in the page's light cover quad (its tint made white:
`FrontendProject::SetSpriteColor`). The confirm key buys (coins spent; refused without
enough) or, had already, takes it ("Change"): an outfit is worn, a car is left at its
seller's spot with the player at its wheel and becomes theirs; Escape leaves.

All the rewards are read in one place now, cars and outfits, their prices and shops
(`Game::rewards`), for the scrapbook, the phone booth and the shops.

Checked: level 1's two sellers (Gil: the Duff Truck and another, 125 coins; Barney: the
Plow King, "Change" with it bought) and the school's wardrobe (three outfits, "Homer -
Casual" 100 coins), by screenshots; a purchase at Gil's with 500 coins: the Duff Truck
bought for 125 (375 left), left at `gil_car`, the player at its wheel.

## The game's HUD (2026-10-21)

The HUD is the game's own page now (`ingame.p3d`'s `Hud.pag`, and `Coins.pag`), drawn by
`UI/GameHud` from the same `HudState` the old one read; the old one (`UI/Hud`) still
draws the call-outs the page has no place for (a mission's title card, alerts) and is
used whole when the page does not load. The control hints are gone, but for the help
overlay's and those that say what is at hand ("Press E to enter vehicle!").

- The radar, bottom right: the page's frame, the player's arrow in its middle, and
  about it the page's icons, each kind from its pool of sprites: the mission's cars, the
  police (kept on the edge when far), the objective's blue flag (likewise), a race's
  checkered flags, the phone booths, the shops (car sellers and wardrobes, "$"), the
  bonus missions, the street races, the player's own car while on foot. The page's map
  is a 3D object the game drew under it; the roads are drawn as dots instead. Ahead is
  up, `radarRange` metres to its rim.
- Round it, the Hit & Run meter: its eight sectors (one picture, `hrsector`, the page
  placing all eight alike: the game turned them), lit an eighth of the meter each,
  turned 45 degrees apiece; under it, at the wheel, the speed in three digits and "kmh".
- Top left, one under the other: the stage's clock ("1:57"), the place in a race
  ("2ND"), its laps ("LAP 1/2"), and in a mission car its damage, the meter's grey bar
  at the car's health. Top right, the coins ("$65"), for four seconds after they change.
  These, and the countdown in the middle, are in the page's letters: the counters'
  multi-sprites hold a picture a character (`letters/0.png`, `letterA.png`,
  `colon.png`...), which the game draws one after another; `GameHud::drawLetters`
  does, each its own width at the counter's height, a fifth of it overlapping.
- Bottom left, the objective: its box (`helptext`), its text wrapped in it, the stage's
  icon (`SetHUDIcon`, now kept by the script engine: `msnicons/<char|location|object|
  vehicle>/<name>.png`), for five seconds after the objective changes. The subtitles go
  above it while it is up, and keep to the left half, clear of the radar.

Checked by screenshots: on foot (radar, phone booths, the Kwik-E-Mart's objective and
icon), a timed mission ("3:21", the couplings' objective wrapped), at the wheel ("1:57",
"2ND", "LAP 1/2", the damage at 60 %, "$65", "000 kmh").

The page's call-outs (2026-10-21): Hit & Run and "BUSTED" (the ticket and its line,
from the alert's term); a card found (its picture, `cards/db/cardNN_t.png`: the page
names blank stubs; its title, "COLLECTOR CARD!"), the level's cards all found
("LEVEL CARDS COLLECTED!"); the wasp cameras' and gags' count ("20/20", in the page's
letters, top right) and their sets done ("LEVEL WASPS COMPLETE!"); a stage announced
complete ("TASK COMPLETE!", two seconds). The old banners are not drawn for them.
Checked by screenshots of each, raised by hand.

Not done: the radar's map, its cone; mission complete and failed in the page's letters,
the proximity meter, the action button, the tutorial's walkie-talkie.
