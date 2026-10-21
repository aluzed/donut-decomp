# donut — Backlog (single source of truth)

> This file replaces the 11 fragmented ticket files and their **3 contradictory numbering
> schemes**: the per-module `TICKETS_*.md` (G1), `MICRO_TICKETS.md` (G2) and
> `TICKETS_RESTANTS.md` (G3). Old IDs are mapped at the bottom (§ E).
>
> Canonical IDs are the per-module prefixes (G1). Detailed tickets live in `docs/tickets/`.
>
> **Statuses checked against the code** (last full pass: 2026-10-18), not against older docs.

## Legend

| Status | Meaning |
|--------|---------|
| ✅ DONE | Implemented and wired in, checked in the code |
| 🟡 PARTIAL | Works, with the missing pieces listed |
| ⬜ TODO | Absent, commented out or an empty stub |
| 🧩 EPIC | Too big: split into sub-tickets below (§ C) |

## Where things stand (2026-10-18)

Every level (1 to 7) loads from the game's own files, and the whole story plays through
from level 1 to the end of level 7 under `--autopilot`, with no crash over runs of ~22
minutes per level. What is missing is mostly **presentation** (animated props, the game's
real menus, subtitles, particles, lighting) and **scale** (streaming, culling).

| | § B tickets | § C epic sub-tickets |
|---|---|---|
| ✅ Done | 20 | 5 |
| 🟡 Partial | 19 | 8 |
| ⬜ To do | 25 | 10 |

Plus 19 closed earlier (§ A) and 4 epics (§ C).

---

## A. Closed — delivered (do not redo)

| Old ID(s) | Subject | Evidence |
|---|---|---|
| RENDER-001/004/005 (G1) | Skeletal animation (`Track::Evaluate`, lerp/slerp, `UpdatePose`, GPU upload) | `Render/SkinAnimation.*`, `Character.cpp` |
| RENDER-002/003 (G1) | `LineRenderer::DrawBox` (3 variants + AABBox) | `LineRenderer.cpp` |
| RENDER-010 (G1) | World and skin fog | shaders |
| CORE-001..010 (G1) | Math bugs in Vector2/4, Matrix3x3 inverse, Quaternion Euler | animation and cameras work |
| CORE-013 (G1) | RZ decompression (P3DZ: see P3D-016) | `P3DFile` |
| PHYS-001/002/003 (G1) | Simulation stepping, full CharacterController, static collision | `CharacterController.cpp`, `Level.cpp` |
| CHARCTRL-004 (G3) | stepUp as a convex sweep | `CharacterController.cpp` |
| PHYS-007 (G1) | `btRaycastVehicle` car (4 wheels, suspension, boost, jump) | `Vehicle.cpp` |
| GAME-004/006 (G1) | Third-person controls, follow/orbit/free cameras | `Game.cpp`, `FreeCamera.cpp` |
| GAME-013 (G1) | DynaLoadData parsing | `Level.cpp` |
| SCRIPT-001/002/003 (G1) | ScriptEngine and the mission state machine | `Scripting/ScriptEngine.cpp` |
| GAME-007/008 (G1), POLICE-* (G2), AI-003/004 (G3) | Traffic, pedestrians, ChaseManager | `AI/` |
| AUDIO-001/003 (G1) | 24-source pool, spatial audio, procedural sounds, dialogue queue | `Audio/AudioManager.cpp` |

### Blockers (P0), all closed

| ID | Subject |
|---|---|
| REGR-001 | The gameplay loop, commented out to isolate a crash, is back on |
| CHARCTRL-FALL | The character fell through the ground: no `btGhostPairCallback` on the broadphase |
| LEVEL-MISSIONP3D | No mission locator was loaded (`art/missions/levelNN/*.p3d`) |
| SCRIPT-PARSE | `find_first_of("//")` cut every `.con` line with a `/` in an argument |
| MISSION-LOOP | Each `.con` completed its own mission at the end of parsing |
| UI-SPLASH | No 2D text or debug lines drawn (attribute locations left to the GL linker) |

---

## B. Open backlog, by module

### Missions and scripts

| ID | Status | Pri | Subject | Note |
|---|---|---|---|---|
| MISSION-STAGES | ✅ | P1 | Missions as sequences of stages | Every objective type the seven levels use works, `destroyboss` included (2026-10-20): level 7's saucer hovers over the playground with its beam, a barrel driven into the beam hits it, the fourth brings it down. Conditions, NPCs, spoken conversations, interiors, retries from `RESET_TO_HERE`. Spoken lines subtitled (AUDIO-007); lip sync is AUDIO-007's. See the ticket |
| LEVELS | ✅ | P1 | Every level, not just level 1 | `--level n`, P3DZ, a level after another in one session, the Sunday drives, the story end to end. Everything is loaded at once; streaming was closed as not needed on PC (LEVEL-STREAM) |
| AI-RACE | ✅ | P2 | Race opponents and rubber-banding | Race and chase cars drive real circuits routed through the roads; a car that cannot get through a stretch is put further along it. Heavy cars no longer sink onto their chassis (suspension force by mass): level 6's truck chase is won (2026-10-21) |
| AI-PATH | ✅ | P2 | Reusable path following | `src/AI/PathFollower.*`, A\* in `PathGraph::FindRoute`, routing by the 966 `RoadSegment`s; the traffic steers with the shared `Steering::Seek`; road links cut into steps of 12m at most (2026-10-21) |
| HIT-AND-RUN | ✅ | P2 | The meter, the police, busted | Running people down and ramming the traffic fill it; full, the level's police cars (`SetNumChaseCars`: one, two from level 4) come one after another, sirens wailing; caught, busted (50-coin fine, mission failed); shaken off once they have closed in. `SetChaseSpawnRate` and `SetHitNRun` are used only by the E3 demo's scripts |

### Game flow and collectibles

| ID | Status | Pri | Subject | Note |
|---|---|---|---|---|
| GAME-003 | ✅ | P1 | The full state machine | Splash, main menu (the game's own `MainMenu.pag` over the Simpsons' living room, one entry between its arrows), the game's loading screen with the level's picture, in game, the game's pause screen, mission complete and failed, the briefing picture on a mission's card, the credits at the story's end, the options, their sound, display, controller, keyboard and game settings pages (kept) work. So do the scrapbook (its pages, stats, contents and four galleries: cards, missions, vehicles, clothes) and the living room (Homer asleep before the television, the entry's object aglow, the race car going round, the camera flying to each options page's object). View Movies too (UI-FMV). The phone booth's car select, the shops (Gil's cars, the wardrobes) and the game's HUD page (radar and its map, counters, call-outs) too. So do the pause between missions (mission select, level progress, cards), Abort Mission, the level's end, level 1's tutorial, and the bonus game (2026-10-21: its menu, seven tracks either way round, four cars, the computer, nitros, wrenches, jumps, up to four players on pads, its pause and standings). The consoles' screens (memory cards, pads) are not the PC's |
| GAME-010 | ✅ | P2 | Collectibles and their saved state | Coins (purse in the save; those broken objects spill), cards (the save), wrenches, phone booths, doorbells, gags (sound, coin, animated model and characters, count, `donut_gags.dat`; in their building, `GagSetInterior`; left as they ended, `GagSetPersist`), wasp cameras (GAME-WASP), buying cars and outfits. The gag commands left are SCRIPT-I's |
| GAME-011 | ✅ | P2 | Full save and load | One save, `donut_game.dat`: the story reached, coins, cards, gags, wasp cameras, bonus missions and street races won, each level's cars and clothes bought and those in use, best times; the separate files of before imported. Not the player's position: the next mission places them |
| GAME-014 | ✅ | P1 | Interiors | Every level's interiors load with the level; doors and zone triggers take the player in and out. Not streamed, as nothing is (LEVEL-STREAM closed: not needed on PC) |
| GAME-WASP | ✅ | P2 | Wasp cameras | Twenty per level hover at their spawn points; drawn to the player and firing at them as the level script says; a jump or a car destroys one (its explosion plays), for 20 coins, kept in `donut_wasps.dat`. Their shots fly and can be dodged; their lens and signal glow; they buzz, fire, zap and blow up with the game's sounds. they dodge about (EVADE_PLAYER); their particle effects run | 137 of 140 in reach (2026-10-21): the double jump, and level 4's Burns estate (`l4r4b`) loaded; three measured still out of reach.

### P3D chunks

| ID | Status | Pri | Subject |
|---|---|---|---|
| P3D-004 | ✅ | P1 | Animation controllers. Pose animations play on composite models (`CompositeModel::Pose`), Vector1/2/3 and (compressed) quaternion channels: gags animate. Gag scenes' characters too (`SceneSkins`). The level's `Anim` and `AnimCollision` objects, never loaded before (69 on level 1: windmill, signs, butterflies, rivers, the power plant's platforms and fans), are drawn and go round. Doorbells play their button and switch when rung; breakables (crates, gas pumps) break when the car runs into them. The moving objects' collision moves with them (the power plant's platforms carry the player). The action triggers' objects too: the power plant's switched and automatic platforms and door, level 7's trapdoors (2026-10-21). Texture animations and blinks play. The controllers and vertex key frames described in `p3d.json` and read through the generated classes (2026-10-21) |
| P3D-001 | ✅ | P1 | Particle systems: the animated objects' effects (steam, smoke, the tire fire, torches), the wasp cameras', breakings', the props' hit effects, the wheels' sprays, a wrecked car's smoke; gravity read right; `RAT` particles a frame up to `NUM` alive; a strip's frames one a sprite; spin (2026-10-20). Weight (gravity scaled, over life) and drag (speed kept a second) too (2026-10-21). Nitro is the mini-game's |
| P3D-006 | ✅ | P1 | Entity variants: `InstancedStaticEntity` drawn (level 3's and 6's bushes, the street races' tyres and barriers: 85 were skipped); `Anim`/`AnimCollision` animated with their collision; `AnimDynamicPhysics` breakable. Their description in `p3d.json` is TOOL-004's |
| P3D-008 | ✅ | — | Spline / LocatorMatrix / SurfaceTypeList | Layouts reversed and checked over 7892 occurrences, 13 camera rails exposed by name; `SurfaceTypeList` drives the grip, sprays, skid marks and footsteps |
| P3D-009 | ✅ | — | CollisionWallVolume / CollisionEffect / Atc | `CollisionEffect` and `Atc` read by hand and used (PHYS-011); no `CollisionWallVolume` in the game. Not in `p3d.json` |
| P3D-002 | ✅ | P2 | Lights (headlights, dynamic lights) | The levels' light group "sun" (an ambient and three directional lights) lights the cars, by their materials' LIT, ambient, diffuse and emissive colours, and the characters; the world is lit by its vertex colours (2026-10-21). Cars' bodies no longer see-through; uniforms not in a program no longer write location 0. The cars' environment maps not done |
| P3D-007 | ✅ | P2 | Tree/TreeNode (spatial, needed for culling) | Read by `P3D::SpatialTree` (2026-10-21): the terrains' kd-tree (level 1: 3103 nodes, 1552 leaves, 15 deep), each node's split, box and counts of the world's objects by kind; the level keeps it. Culling by it is RENDER-008b's |
| P3D-010 | ✅ | P2 | StaticProp (6 chunks) | Read by `P3D::StateProp` (2026-10-21): the levels' 263 breakables go state to state on a strike, a kick, a car (a crate takes three strikes, ten coins spilled each), their visibility, pose and particles as each state says, their callbacks called (coins, collision, gone); the wasps read theirs through it |
| P3D-011 | ✅ | P2 | Geometry extras (PackedNormalList, DrawShadow...) | Closed on the data (2026-10-21): no `Mesh`; `PackedNormalList` always beside a full `NormalList` (read, for the lights); `VertexShader` and `DrawShadow` are markers; offsets are the living room's morphs |
| P3D-014 | ✅ | P2 | CompositeDrawableSkin/Effect parsing | Read (2026-10-21): the skins (name, translucent) and effects (name, translucent, joint) of a composite drawable's lists; the breakables' and the wasp's effects go through them. Skins are only the characters', the scenes' and the frontend's, drawn by their own readers |
| P3D-003 | ⬜ | P3 | Facial expression animation |
| P3D-012 | ✅ | P3 | ExportInfo (the Maya exporter's settings): described in `p3d.json`, generated, shown in the P3D debug viewer (2026-10-21) |
| P3D-017 | 🟡 | P3 | Non-PNG images | DDS (DXT1, DXT3, DXT5) decoded: the frontend's level and mission pictures (2026-10-20). Other palettised formats not |
| P3D-005 | ✅ | — | The text bible: read from `srr2.txt` (`UI/TextBible`), in five languages |
| P3D-013 | ✅ | — | `GetShaderTexture()`: a false bug, no such call |
| P3D-016 | ✅ | — | P3DZ: Radical LZ blocks, decompressed in `P3DFile` |

### Render

| ID | Status | Pri | Subject | Note |
|---|---|---|---|---|
| RENDER-006 | ✅ | — | AnimObjectWrapper `_shaders`/`_textures` | No wrapper in the game holds a shader or a texture: their file's own serve (2026-10-20) |
| RENDER-007 | ✅ | P2 | `CompositeDrawable` methods | Closed (2026-10-21): `CompositeModel` does it (props on their joints, visibility, animation); the empty declaration, never defined nor used, removed |
| RENDER-011 | ✅ | P2 | WorldSphere billboards (lens flare, sun halo) | Drawn with the billboard program, not the world's: the sun's warm halo, the sky's clouds (2026-10-19). Its lens flare (2026-10-21): the sun's glares on their skeleton's joints, seen as the camera looks within their range of the sun, dimmed as much as the sun is hidden (an occlusion query, read a frame later) |
| RENDER-014 | ✅ | P2 | Multi-UV (channel 1 = lightmap) | Closed (2026-10-21): no UV list in the data has a second channel (19866 lists in 655 files, all channel 0); the channel is asked for, not caught |
| RENDER-015 | ✅ | P2 | Lightmaps, static lighting | Closed (2026-10-21): the PC data has none: no second UV channel, no shader with a second texture (`TEX` 14053, `REFL` 157); the world is lit by its vertex colours |
| RENDER-008 | ✅ | P2 | LOD and spatial culling | § C.3: frustum culling done; the level's kd-tree tried and measured slower than the linear test (900 entities, 30-40 µs a frame), not kept; the game has no LOD (2026-10-21) |
| RENDER-009 | 🧩 | P3 | Post-process | § C.4 |
| RENDER-013 | ✅ | — | Shader cache | |
| RENDER-CAR | ✅ | — | Cars drawn whole, with the world shader; follow cameras out of the scenery | |
| VEH-DAMAGE | ✅ | — | A damaged car shows its dents (`*Dam.bmp` textures) | |

### Physics

| ID | Status | Pri | Subject | Note |
|---|---|---|---|---|
| PHYS-004 | ✅ | — | Collision callbacks | Contacts read after each step (`GetContacts`): props, breakables; car damage from its change of velocity, with a thud; pickups by distance (2026-10-20) |
| PHYS-011 | ✅ | — | Things to knock over | The `DynamicPhysics` props have bodies (mailboxes and bins fly, lamp posts give way, hydrants and stop signs break, trees shed leaves); the `AnimDynamicPhysics` breakables stand until run into; hit effects, masses and sounds from the game's tables. `eParkingMeter`, `ePopsicles` are in no file of the game (2026-10-20) |
| PHYS-008 | ✅ | — | Grip per surface | Each wheel's friction from the ground's terrain type (`SurfaceTypeList`): grass, dirt, sand slip; the rear wheels throw up grass or earth; skid marks per ground (2026-10-20) |
| PHYS-005 | ✅ | — | `BulletFenceShape` for fences | |
| VEH-SINK | ✅ | — | The car drives, and the camera stays above ground | |
| VEH-WHEELS | ✅ | — | Cars stand on their own wheels | Physics wheels at each model's hubs and radii, drawn with their suspension travel; traffic rides at each model's height (2026-10-21) |
| CHAR-FEET | ✅ | P1 | Characters on their feet | Drawn at their feet, not their capsule's middle (0.875m up); pedestrians no longer step onto awnings (2026-10-21) |
| — | ✅ | — | Traffic stops short of cars and of the player on foot (kinematic bodies pushed through them) | |
| — | ✅ | — | A car that falls off the map is put back on the nearest road | |

### Audio

| ID | Status | Pri | Subject | Note |
|---|---|---|---|---|
| AUDIO-002 | ✅ | P1 | Music | The `l<n>_music.rms` script read by its names: exploring, missions (start, win, lose, out of the car), races, bonus, Hit & Run. Not on the beat. music02.rcf mended by tools/extract_assets.sh |
| AUDIO-007 | ✅ | P2 | Dialogue: subtitles and lip sync | Subtitles heard by machine (tools/transcribe_dialogue.py), a setting to hide them (2026-10-21), the movies' lines in each soundtrack's language (tools/transcribe_movies.py), and lip sync: the speaker's jaw opened by the loudness of the line (2026-10-21) |
| AUDIO-008 | ⬜ | P3 | EFX reverb per zone (inside/outside) | |
| AUDIO-010 | ✅ | — | Footsteps and tyre squeal by the ground | The characters' own steps (concrete, metal, wood, dirt), landings; `skid`/`dirt_skid` looped while the player's car skids (2026-10-20) |
| AUDIO-006 | ✅ | — | `audio/` scanned for RCFs, any case | |
| AUDIO-009 | ✅ | — | Horns: the player's, and the traffic's when kept waiting | |
| — | ✅ | — | Engines pitched to the revs, ambience per level, gag sounds | none of it listened to yet on the dev machine |

### Level

| ID | Status | Pri | Subject | Note |
|---|---|---|---|---|
| LEVEL-STREAM | ✅ | P1 | Load and unload by distance | Closed, not needed on PC: everything loaded, each level is under 360 MB and 2.5 s to load (measured 2026-10-20); the game streamed to fit the PS2's 32 MB |
| LEVEL-FOG | ✅ | P2 | Fog per zone from P3D parameters | Closed: the game has no fog. No fog in any level file; `Simpsons.exe` builds its world render layer with fog off and never turns it on (Ghidra, 2026-10-21). Our invented haze is off |
| LEVEL-TRIG | ✅ | — | Triggers act | Doors and zone interiors, doorbells, phone booths, wrenches, bonus givers, gags. The dynamic-zone loaders wait for LEVEL-STREAM |

### UI / HUD

| ID | Status | Pri | Subject | Note |
|---|---|---|---|---|
| UI-TEXT | ✅ | — | Frontend MultiText | The frontend pages' texts drawn: the text bible's phrase, the page's font, aligned and fitted to their box; the pages' images loaded from `resource/` (2026-10-20) |
| UI-LOADING | ⬜ | P3 | Loading screen and hints | |
| UI-FMV | ✅ | P3 | The game's movies | Bink, decoded with FFmpeg (optional): the logos and the introduction at start-up, level 1's on a new game, the story's `fmv` stages, the gags' (telescope, Larry the Looter, the Aztec's ticket), View Movies. Not the credits' movie |
| UI-MENU | ✅ | — | Main and pause menus, hit-testing, pause freezes the game | |
| UI-HUD | ✅ | — | Health, Hit & Run, nitro, speedometer, coins, cards, objective, laps, timer, radar with roads | `src/UI/Hud.*` |
| UI-LANG | ✅ | — | The game's language | `--language E/F/G/I/S`, or the options' Language row (kept in `donut_settings.dat`); the engine's own words in the five languages (`UI/GameText`); "???" falls back to English (2026-10-21) | |
| UI-TEXTFIT | ✅ | P1 | Objectives fit their frame | Wrapped text sized to its box's height as well as its width (2026-10-21) |
| UI-RACE-GUIDE | ✅ | — | Checkpoint markers, next checkpoint on the radar and the HUD | |

### Tools and assets

| ID | Status | Pri | Subject |
|---|---|---|---|
| ASSET-001..006 | ✅ | — | Extraction pipeline to the game tree | `tools/extract_assets.sh <dir of the CD images>`: the cabinets off the three ISOs (or their .7z), unshield, then `art/`, `scripts/`, DIALOGF.RCF in place; unshield and 7z fetched without root if missing (2026-10-20) |
| TOOL-002 | ✅ | P2 | Reverse `SIMPSONS.EXE` (Ghidra) | `tools/ghidra/analyze.sh`: functions, strings, classes, decompiled C, 1605 vtables from the RTTI; anchors in `docs/GHIDRA.md` (2026-10-21) |
| TOOL-004 | ✅ | P2 | Run `donut-codegen`, enrich `p3d.json` | `dev/codegen/generate.sh` regenerates `P3D.generated.*` and `Commands.generated.cpp` exactly as committed (.NET 8, type remap, clang-format, `hand-edits.patch`; `--check`); `p3d.json` holds the 113 generated classes, the 25 unused ones in `p3d.unused.json` (2026-10-21) |
| TOOL-007 | ⬜ | P3 | P3D unit tests (parse, reserialise, compare) |
| TOOL-010 | ⬜ | P3 | CI (Linux, Windows) and clang-format |
| TOOL-008 | ✅ | — | `.con` format documented (`SCRIPT_FORMAT.md`) |
| CORE-014/015 | ✅ | — | Logging to stderr by level; stray includes removed |

---

## C. Epics

### C.1 — GAME-001: split up `Game`

`Game.cpp` has grown from 1499 to **3353 lines** (2026-10-18): levels, story, autopilot,
interiors, gags, Hit & Run, buying, audio cues all live there. Each step must build and play
the same.

| Sub-ticket | Status | Subject |
|---|---|---|
| GAME-001a | ✅ | `GameInput` (input to intents) |
| GAME-001b | ✅ | `GameCamera` (follow, orbit, camera moves, conversation shots, shake) (2026-10-21) |
| GAME-001c | ✅ | 2D drawing out of `Game::Run`: the HUD in `UI/Hud.*`, menus, results and hints in `UI/ScreenText` (2026-10-21) |
| GAME-001d | ✅ | `GameStateMachine`: the states' transitions from one table of events (2026-10-21) |
| GAME-001e | ✅ | `Run` and the constructor as their phases (137 lines); `GagManager`; levels, story, interiors and the autopilot in files of their own; members grouped, 190 -> 142 (2026-10-21) |

### C.2 — SCRIPT: `.con`/`.mfk` commands

170 of the 247 commands in `GameCommands.cpp` are still empty one-line stubs (215 in
2026-08). That overstates what is missing: several categories are read straight from the
script files by the system that needs them (gags, traffic models, Hit & Run, the player's
car, rewards), not through the commands.

| Sub-ticket | Status | Category |
|---|---|---|
| SCRIPT-A | ✅ | The base race commands: every level's races run |
| SCRIPT-B | ✅ | Car tuning: every command of the 90 car scripts applied or kept; lock, brakes, springs, centre of mass, hit points read against the median car (2026-10-21) |
| SCRIPT-C | ✅ | Cameras: conversation shots line by line (`SetConversationCam`, `SetCamBestSide`), the missions' and races' camera moves as they start (2026-10-21) |
| SCRIPT-D | ✅ | Race tuning: laps, catch-up, the opponent's win, `SetConditionPosition`, the chased car's catch-up; `SetVehicleAIParams` kept, its meaning unknown (2026-10-21) |
| SCRIPT-E | ✅ | Gags and collectibles: read from `level.mfk` (GAME-010); state props for `pickupitem` |
| SCRIPT-F | ✅ | Hit & Run: decay and chase cars read from `leveli.mfk`, `DisableHitAndRun`, `SetHitNRun`; the rest in no script (2026-10-21) |
| SCRIPT-G | ✅ | Traffic and pedestrians: `SetMaxTraffic` caps the cars, `UsePedGroup` picks the level script's pedestrian group (2026-10-21) |
| SCRIPT-H | ✅ | UI: `SetPresentationBitmap` shown; `SetFadeOut` and `SetIrisWipe` close the screen at a stage's end, `ShowHUD` hides the HUD. The car swaps behind the fades are stubs |
| SCRIPT-I | 🟡 | Buying, dialogue, bonus missions, FMV done; `GagCheckCollCards`, `GagSetSparkle`, `GagSetAnimCollision`, `GagSetIntro`/`Outro`, `GagBinding(s)` not |

### C.3 — RENDER-008: LOD and culling

| Sub-ticket | Status | Subject |
|---|---|---|
| RENDER-008a | ✅ | Frustum culling per drawable bounding box |
| RENDER-008b | ✅ | Spatial tree from the `Tree` chunks: tried, slower than the linear test, not kept (2026-10-21) |
| RENDER-008c | ✅ | LOD by distance: closed, the game has no LOD (2026-10-21) |

### C.4 — RENDER-009: post-process

| Sub-ticket | Status | Subject |
|---|---|---|
| FX-000 | ✅ | FBO pipeline (scene to `_sceneFBO` to a full-screen quad) |
| FX-002 | ✅ | Vignette |
| FX-005 | ⬜ | LUT colour grading |
| FX-001 | ⬜ | Bloom |
| FX-004 | ⬜ | Shadow mapping (ex RENDER-012) |
| FX-003 | ⬜ | Motion blur |

---

## D. Suggested order

1. **Animated props** (P3D-004, then RENDER-006/007): gags, doors and level props come to
   life; the biggest visible gain.
2. **The game's own screens** (GAME-003, UI-TEXT, UI-LOADING, AUDIO-007 subtitles).
3. **Gameplay left**: one save slot (GAME-011, done).
4. **World look**: lights (P3D-002), lightmaps (RENDER-014/015), fog (LEVEL-FOG),
   sun halo (RENDER-011).
5. **Scale**: nothing left. The spatial tree (RENDER-008b) was slower than the linear
   frustum test at 900 entities a level; LEVEL-STREAM was closed: under 360 MB and 2.5 s
   a level with everything loaded.
6. **Debt** (in parallel): GAME-001b..e, CI and tests (TOOL-007/010).

---

## E. Old IDs to canonical

| Old (generation) | Canonical |
|---|---|
| RENDER-001 (G3 "Lightmap") | RENDER-015 |
| RENDER-001 (G1 "SkinAnimation") | closed (§ A) |
| RENDER-002/003 (G3 culling/LOD) | RENDER-008 (§ C.3) |
| RENDER-002 (G1 DrawBox) | closed |
| UI-001 (G2 "mouse click") / UI-003 (G1) | UI-MENU |
| UI-001 (G3 "page nav") / UI-002 (G1) | GAME-003 + UI-MENU |
| UI-003/004 (G2 states/text) / UI-004 (G1) | UI-TEXT |
| UI-004 (G2 HUD) / UI-005 (G1) | UI-HUD |
| AUDIO-001 (G2/G3 streaming) / AUDIO-002 (G1) | AUDIO-002 |
| AUDIO-002 (G3 subtitles) / AUDIO-007 (G1) | AUDIO-007 |
| AUDIO-004 (G2 hash) / AUDIO-001 (G1 pool) | closed |
| PHYS-003 (G3 rigid bodies) | PHYS-011 |
| PHYS-004 (G3 callbacks) | PHYS-004 |
| PHYS-005 (G3 friction) / PHYS-008 (G1) | PHYS-008 |
| CHARCTRL-004 (G3 stepUp) | closed |
| VEH-005 (G3) / GAME-012 (G1) | VEH-DAMAGE |
| LEVEL-003 (G3) | LEVEL-STREAM |
| LEVEL-004 (G3) | LEVEL-TRIG |
| LEVEL-005 (G3) | LEVEL-FOG |
| AI-002 (G3 path) | AI-PATH |
| AI-005 (G3 race) | AI-RACE |
| AI-003/004 (G3), POLICE-* (G2) | closed |
| SCRIPT-004..010 (G1 by category) | SCRIPT-A..I (§ C.2) |
| ASSET-001..006 (G2) | ASSET-001..006 |
| FX-001..005 (G2) | FX-* (§ C.4) |
| GAME-001 (G1 refactor) | GAME-001a..e (§ C.1) |
