# Remaining Tickets — Final Audit

## Actual State

| System | Completion | Verdict |
|---------|-----------|---------|
| P3D/RCL Parsing | 85% | Solid |
| Rendering | 75% | Working, no post-FX |
| Physics | 80% | Static + dynamic working |
| Audio | 70% | Source pool, spatial, procedural |
| Scripting | 60% | Parser + 40/218 commands |
| Game Flow | 80% | State machine, HUD, mission loop |
| Character Ctrl | 75% | Movement, jump, ground detection |
| Vehicle | 70% | Physics, input, enter/exit |
| AI/Traffic | 50% | Traffic, race opponent, police |
| UI/HUD | 60% | HUD complete, menus WIP |
| Save/Load | 30% | Best time persistence |

## Remaining Tickets by Category

### CHARCTRL — Character Controller (1 ticket)
- **CHARCTRL-004**: Replace naive `stepUp()` with sweep test + ceiling detection

### VEH — Vehicle (1 ticket)
- **VEH-005**: Visual damage (swap car_damage.p3d)

### LEVEL — Level Loading (3 tickets)
- **LEVEL-003**: Distance-based streaming (load/unload based on player position)
- **LEVEL-004**: Trigger system (Locator2/TriggerVolume → script callbacks)
- **LEVEL-005**: Per-zone fog from P3D parameters

### SCRIPT — Scripting (2 tickets)
- **SCRIPT-004**: Traffic/pedestrian spawn commands
- **SCRIPT-005**: Gags/collectibles spawn system

### GAME — Game Flow (2 tickets)
- **GAME-004**: Pause menu overlay with options
- **GAME-005**: Win/lose detection (objectives, conditions, death)

### AI — Artificial Intelligence (4 tickets)
- **AI-002**: Path-following controller (steer toward waypoint)
- **AI-003**: Pedestrian spawning with wander behavior
- **AI-004**: ChaseManager — pursue player with heat system
- **AI-005**: Race AI — opponents with rubber-banding

### AUDIO — Audio (4 tickets)
- **AUDIO-001**: Dialogue priority queue (cutscene > gameplay)
- **AUDIO-002**: Synchronized subtitles (timed text events)
- **AUDIO-003**: Music streaming (double-buffer OpenAL)
- **AUDIO-004**: Sound hash index (O(1) lookup instead of O(n))

### UI — Interface (5 tickets)
- **UI-001**: Screen/page navigation (active page stack)
- **UI-002**: Mouse picking + buttons (hover/pressed/disabled)
- **UI-003**: MultiText rendering (TextureFont)
- **UI-004**: Core HUD (health, speed, coins)
- **UI-005**: Pause menu overlay

### RENDER — Rendering (4 tickets)
- **RENDER-001**: Lightmap support (2nd UV channel + lightmap texture)
- **RENDER-002**: CompositeModel_AnimObjectWrapper shaders/textures
- **RENDER-003**: Basic frustum culling
- **RENDER-004**: LOD (distance-based model swap)

### PHYS — Physics (3 tickets)
- **PHYS-003**: Dynamic rigid bodies for AnimDynamicPhysics
- **PHYS-004**: Collision callbacks (damage, pickup, sound)
- **PHYS-005**: Surface friction (road vs grass vs sidewalk)

---

**Total remaining: 29 tickets across 9 categories.**

**Critical path** (minimum playable): CHARCTRL-004, VEH-005, GAME-004, AI-004 ≈ **4 tickets**.
