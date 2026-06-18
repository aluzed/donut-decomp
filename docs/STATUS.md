# donut — Engine Status

## From Viewer to Game

This document summarizes the work done to transform donut from a P3D file viewer
into a playable arcade racing game.

## What Was Built

### Core Systems
- **Linux Build**: Native GCC/Clang support with CMake
- **Math Library**: Fixed Vector2/4 bugs, Matrix3x3 Inverse, Quaternion Euler
- **Log System**: Multi-level logging (Debug/Info/Warn/Error) to stdout/stderr
- **P3D Parser**: 165+/182 chunk types supported, RZ compression, flexible paths

### Rendering
- **SimpleMesh**: Procedural 3D geometry (box, capsule) with lighting
- **Fog**: Exponential fog in world + skin shaders, sky color
- **Lightmap Support**: Multi-UV channel ready, shader infrastructure
- **Skeletal Animation**: Keyframe interpolation (lerp+slerp), skinning shader
- **LineRenderer**: Debug drawing for physics, skeletons, beacons

### Physics (Bullet)
- **Character Controller**: Capsule shape, raycast ground detection, jump, stepUp
- **Vehicle Physics**: btRaycastVehicle with 4-wheel suspension
- **Collision**: All P3D volume types, fence shapes, triangle meshes
- **Gravity**: 9.8 m/s² with velocity caps

### Audio (OpenAL)
- **Source Pool**: 24 simultaneous sounds
- **Spatial Audio**: 3D positioning with listener orientation
- **Procedural Sounds**: Beep/Chirp generators (PCM in memory)
- **RCF Loading**: Auto-detect archive files

### Script Engine
- **Parser**: Text-based .con script loader, multi-command lines
- **40+ Commands**: Mission flow, vehicle tuning, objectives
- **Mission State**: SelectMission→AddStage→Checkpoints→CloseMission
- **Locator System**: Name→position mapping from P3D data

### AI
- **PathGraph**: Road network from level Path data, nearest-neighbor graph
- **Traffic**: 12 cars with steering behaviors (seek, arrival)
- **Race Opponent**: Adaptive AI (rubber-banding 6-20 m/s)
- **Police Chase**: ChaseManager with heat system, pursuit, bust/evade

### Gameplay
- **Vehicle System**: Enter/exit, boost (Shift), jump (Space), horn (H)
- **Checkpoint Racing**: 2-lap circuit with 6 checkpoints
- **Best Times**: Persistent save/load to `donut_save.dat`
- **Auto-Restart**: 5-second countdown after win/lose
- **Screen Shake**: On jump, checkpoint, mission fail

### UI/HUD
- **Splash Screen**: Frontend bootup.p3d with "PRESS ENTER TO START"
- **HUD**: FPS, position, HP, timer, objectives, laps, speed, heat
- **Pause Menu**: ESC toggle with Quit option
- **Debug Mode**: Key 1 toggles wireframe/solid rendering

### Camera
- **Third-Person Follow**: Smooth exponential lerp behind character/vehicle
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
| E | Jump / Enter car | Exit car |
| Space | - | Jump |
| Shift | - | Boost |
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
