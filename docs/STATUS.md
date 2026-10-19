# donut — Engine Status

## From Viewer to Game

This document summarizes the work done to transform donut from a P3D file viewer
into a playable arcade racing game.

## What Was Built

### Core Systems
- **Linux Build**: Native GCC/Clang support with CMake
- **Math Library**: Fixed Vector2/4 bugs, Matrix3x3 Inverse, Quaternion Euler
- **Log System**: Multi-level logging (Debug/Info/Warn/Error) to stdout/stderr
- **P3D Parser**: 165+/182 chunk types supported, RZ and Radical's own P3DZ compression,
  flexible paths

### Rendering
- **SimpleMesh**: Procedural 3D geometry (box, capsule) with lighting
- **Fog**: Exponential fog in world + skin shaders, sky color
- **Lightmap Support**: Multi-UV channel ready, shader infrastructure
- **Skeletal Animation**: Keyframe interpolation (lerp+slerp), skinning shader
- **Animated world** (P3D-004): gags with their characters; the level's animated objects
  (windmill, signs, butterflies, the power plant's platforms, whose collision moves with
  them); breakables that break; doorbells; rivers, pools and screens (texture
  animations); flashing sirens and pulsing glows (billboard animations); blinking eyes
- **Characters**: the player runs, dashes, jumps, kicks, lands, fidgets, presses
  doorbells, celebrates, gestures in conversation, gets in and out of cars by the
  driver's door (which opens), and sits at the wheel; mission cars have their drivers;
  pedestrians pause, flail when hit and run clear of cars
- **Cars**: dents, wheels that roll and steer, brake and reversing lights (the traffic's
  too)
- **Particles** (P3D-001): steam, smoke, fires, torches; the wasps' effects; sparks off a
  crash, dust under a landing
- **Wasp cameras**: twenty a level, coming in as the player nears, watching, firing shots
  that can be dodged, blown up by a jump or a car
- **LineRenderer**: Debug drawing for physics, skeletons, beacons

### Physics (Bullet)
- **Character Controller**: Capsule shape, raycast ground detection, jump, stepUp
- **Vehicle Physics**: btRaycastVehicle with 4-wheel suspension
- **Collision**: All P3D volume types, fence shapes, triangle meshes
- **Gravity**: 9.8 m/s² with velocity caps

### Audio (OpenAL)
- **Source Pool**: 24 simultaneous sounds; the listener is the camera
- **Music**: each level's own score, its RSD segments chained in turn
- **Ambience**: each level's background loop outdoors, a room's indoors
- **Cars**: the driven car's own engine loop, pitched with speed, and its own horn
- **Voices**: mission conversations spoken line after line; the player's character on
  crashes, jumps, people run down, cards, wins and failures; traffic drivers rammed,
  residents at their doorbell, the level's people walked up to
- **RCF Loading**: Auto-detect archive files (DIALOGF.RCF included)

### Script Engine
- **Parser**: Text-based .con/.mfk script loader, multi-command lines
- **Stages**: each stage's commands are recorded and run when it begins; objectives
  complete and advance (goto, interior, race, getin, talkto, dialogue, timer, delivery,
  dump, follow, destroy, losetail); conditions fail them (timeout, outofvehicle, damage,
  followdistance, race); a failure is retried from `RESET_TO_HERE` (MISSION-STAGES)
- **Every level**: `--level n` plays any of the seven, from its own files and scripts:
  world, interiors, story, player and car. Winning a level's last mission loads the next
  in the same session; the story ends after level 7. Every level's story plays through
  under `--autopilot` (LEVELS). New Game starts on level 1; Continue resumes the level
- **Car scripts**: every car's mass, engine and top speed from `scripts/cars/<car>.con`
- **Phrase table**: objective lines in the game's own words, in five languages
- **Locator System**: Name→position mapping from P3D data, car-start headings, trigger
  volumes with entry/exit events

### AI
- **PathGraph**: Road network from level Path data, nearest-neighbor graph
- **Traffic**: 12 cars cruising the roads, choosing their way at junctions; they stop short of a car in their way (they are kinematic, and pushed through it) or of the player on foot, and sound their horn when kept waiting
- **Race Opponents**: any number, driving real vehicles over a circuit routed through
  the road network (shortcuts excluded); speed profile, hairpins and three-point turns,
  mission catch-up curves; loops or start-to-finish routes (AI-RACE, SCRIPT-D)
- **Hit & Run**: running people down and ramming the traffic fill a meter; full, a police
  car drives after the player; caught, busted: a 50-coin fine and the mission failed
  (HIT-AND-RUN)

### Gameplay
- **Vehicle System**: Enter/exit, boost (Shift), jump (Space), horn (H)
- **Races**: the mission's own checkpoints and lap count, countdown, race position,
  losing to an opponent who finishes first, races against the clock (par time)
- **Best Times**: one per mission ("L1:gr1 195.2"), kept in `donut_save.dat`
- **Coins**: the level's own coins (177 on level 1), in the game's coin model; a purse
  kept in `donut_coins.dat`, which pays entry fees (gr1)
- **Collector cards**: each level's seven, found for good (`donut_cards.dat`)
- **Gags**: the level's interactive gags (33 on level 1) stand in their rest pose, and play
  their sound when walked into or worked by hand, paying a coin the first time; the count
  found is shown. Their animations are not played
- **Wrenches and phone booths**: a wrench repairs the car; a booth sends the player's car,
  repaired; a doorbell rung is answered
- **Street races and bonus missions**: offered by their characters about the level, the
  story taken up again afterwards
- **Buying**: cars and outfits with coins, at the game's prices
- **People**: the level's ambient characters stand about (Ralph, the Moleman...), and
  mission NPCs and they turn to face the player
- **Car damage**: from impacts, judged by the car's change of velocity; a mission's
  "damage" condition fails it, with the game's reason and hint on the failure screen.
  The player's car shows its dents: hood, doors and back swap to their damaged textures
- **Pedestrians**: Springfield's people, walking, outdoors only
- **Auto-Restart**: 5-second countdown after win/lose
- **Falling off the map**: a car is put back on the road nearest where it last stood, the
  player at the wheel; on foot, the player is set down where they last walked
- **Screen Shake**: On jump, checkpoint, mission fail

### UI/HUD
- **`--clean`**: the game as it is played, without the developer's windows, lines and
  skeletons
- **Splash Screen**: Frontend bootup.p3d with "PRESS ENTER TO START"
- **HUD**: FPS, position, HP, timer, objectives, laps, speed, heat
- **Pause Menu**: ESC toggle with Quit option
- **Debug Mode**: Key 1 toggles wireframe/solid rendering

### Camera
- **Third-Person Follow**: Smooth exponential lerp behind character/vehicle, looking at
  its subject at any heading (the view rotation is the camera's inverse)
- **Freecam**: Right-click for WASD fly mode
- **Smooth Interpolation**: exp(-8.0*dt) easing

### Tools
- **CAB Extraction**: unshield-based extraction of InstallShield archives
- **ISO Extraction**: 7z-based game CD extraction
- **Procedural Content**: Buildings, traffic, road markings

## Controls

| Key | On Foot | In Vehicle |
|-----|---------|------------|
| Arrows | Move | Drive |
| E | Jump / Enter car / kick in the air | Exit car |
| Space | - | Jump |
| Shift | Dash | Boost |
| H | - | Horn |
| M | Restart mission | - |
| T | Teleport to vehicle | - |
| 1 | Toggle debug view | - |
| ESC | Pause | Pause |
| Right Click | Freecam fly | Freecam fly |

## Game Loop

```
Splash (Press ENTER)
  → GO! (1.5s)
  → Race (2 laps, 6 checkpoints, timer)
  → STAGE COMPLETE! / MISSION FAILED!
  → Auto-restart (5s)
```

## Dependencies

```
sudo apt install cmake build-essential git \
  libsdl2-dev libopenal-dev libbullet-dev libfmt-dev libglm-dev \
  libgl1-mesa-dev mesa-utils
```

## Build

```bash
git clone --recursive https://github.com/aluzed/donut.git
cd donut
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/bin/donut
```

## Command Line

| Flag | Effect |
|------|--------|
| `--log-level <debug\|info\|warn\|error>`, `-l` | Log verbosity (or the `DONUT_LOG_LEVEL` env var) |
| `--autostart` | Skip the splash and main menu, straight into the mission |
| `--quit-after <secs>` | Close the window after that much wall time |
| `--mission <script>` | Run that mission script instead of `M1race.con`. Given a game set-up script (`<id>i.mfk`), its loader (`<id>l.mfk`) runs first |
| `--autopilot` | Move the player onto each objective in turn, to run a whole mission unattended |
| `--story` | Start on level 1's first story mission (m0) and go on to the next after each win, through m7. Needs `scripts/level01` (see docs/ASSETS.md) |
| `--language <E\|F\|G\|I\|S>` | Language of the game's own text (objectives), from its phrase table. English by default |

The last two make a run repeatable without a keyboard, which is how the race AI is measured
over a whole 120-second mission:

```bash
./build/bin/donut --log-level debug --autostart --quit-after 140 > run.log
```

The game's own street race, three AI cars over three laps:

```bash
./build/bin/donut --autostart --quit-after 150 \
    --mission files/extracted/Scripts/missions/level01/sr2i.mfk > sr2.log
```

## Asset Extraction

```bash
# Requires game CDs
./tools/extract_cab.sh
```

Extracts P3D models, levels, and characters from InstallShield CABs.

## Limitations

- Frontend/audio assets require full multi-volume CAB extraction (CD2+CD3)
- Police AI is basic (no road following)
- No gamepad support
- No save/load except best time
