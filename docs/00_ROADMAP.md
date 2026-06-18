# donut Engine - Reverse Engineering Roadmap

## Status

**Project:** Open source reimplementation of *The Simpsons: Hit & Run* (2003, PC)
**Original Engine:** Radical Engine (Pure3D + RCL)
**Language:** C++17 / OpenGL 4.3
**Assets Required:** Original game files (.p3d, .rcf, .rsd, .con scripts)

### Existing Modules

| Module | Status | Files | P3D Chunks Parsed | Notes |
|--------|--------|-------|-------------------|-------|
| **P3D** | 90% | 6 | 165+/182 | Missing ~17 chunk types (particles, lights, etc.) |
| **RCL** | 100% | 4 | N/A | RCF archive + RSD audio complete |
| **Core** | 95% | 18 | N/A | Minor bugs fixed (Vector4, Quaternion, Matrix3x3) |
| **Render** | 80% | 32 | N/A | SkinAnimation and DrawBox fixed |
| **Physics** | 80% | 8 | N/A | stepSimulation active, CharacterController working |
| **Audio** | 70% | 2 | N/A | 24-source pool, spatial audio, procedural sounds |
| **Scripting** | 60% | 4 | N/A | 185 commands defined, 40+ implemented |
| **AI** | 50% | 4 | N/A | Traffic, race opponent, police chase |

### Available Assets
- `files/`: 3 game CDs (ISO in .7z)
  - CD1: SIMPSONS.EXE (22KB launcher), DIALOGF.RCF (159MB), MOVIES/, data1.cab + data2.cab
  - CD2/CD3: Game assets (P3D, RCF, Scripts) - CD2 has data3.cab (650MB), CD3 has data4.cab (606MB)

---

## Target Architecture

```
donut
├── Core/           # Math types, I/O, memory, logging
├── P3D/            # Pure3D format parsing (~182 chunk types)
├── RCL/            # RCF archives, RSD/RADP audio
├── Render/         # OpenGL 4.3: meshes, shaders, skinning, sprites, post-FX
├── Physics/        # Bullet Physics integration
├── Audio/          # OpenAL: spatial audio, music, dialogue
├── Scripting/      # Script engine executing .con missions
├── AI/             # Traffic, pathfinding, police, race AI
├── Game/           # Gameplay: characters, vehicles, missions
├── UI/             # Frontend (menus, HUD)
├── Input/          # Keyboard/mouse/gamepad
└── docs/           # Documentation and tickets
```

---

## Phases and Priorities

### Phase 1: Foundation ✅
Fix critical bugs in math, complete P3D parsing.

### Phase 2: Rendering + Physics ✅
Fully functional viewer: all entities, animations, active physics.

### Phase 3: Basic Gameplay ✅
Playable character, vehicles, collisions, game camera.

### Phase 4: Mission System ✅
Script VM, .con execution, basic AI.

### Phase 5: Polish (in progress)
Spatial audio, post-processing, complete UI, gamepad support.

---

## Ticket Files

See detailed tickets per module:
- TICKETS_CORE.md — Math bugs, P3DZ/RZ
- TICKETS_P3D.md — Missing P3D chunks, codegen
- TICKETS_RENDER.md — LOD, post-FX
- TICKETS_PHYSICS.md — Vehicle physics, CharacterController
- TICKETS_AUDIO.md — Streaming, spatial audio
- TICKETS_SCRIPTING.md — Script commands
- TICKETS_GAMEPLAY.md — Player, vehicles, AI
- TICKETS_UI.md — Frontend, HUD
- TICKETS_TOOLS.md — Tools, codegen, Ghidra
