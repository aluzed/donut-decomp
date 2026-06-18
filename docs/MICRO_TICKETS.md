# Micro-Tickets — Remaining Tasks

## 1. Asset Extraction (6 tickets)

### ASSET-001 — Build unshield statically
- Build `unshield` from https://github.com/twogood/unshield
- Verify `unshield -d out x data1.cab` works
- **~5 min**

### ASSET-002 — Extract data1.cab + data2.cab from CD1
- Use `7z e` to extract CABs from the ISO
- Place in `assets/`
- **~10 min**

### ASSET-003 — Test extraction of a P3D file
- `unshield x data1.cab` → verify `Art/b00.p3d` is extracted
- Open the file with `P3DFile` in donut
- **~15 min**

### ASSET-004 — Extract all CAB files
- Run full extraction
- Organize into `art/`, `audio/`, `movies/` tree
- **~20 min**

### ASSET-005 — Load a real character model
- Replace `SimpleMesh` with P3D loaded from `art/chars/homer_m.p3d`
- Verify skinning + animations work
- **~30 min**

### ASSET-006 — Load real game textures and shaders
- Load real textures from CAB P3Ds
- Verify P3D shaders are correctly interpreted
- **~30 min**

---

## 2. Police/Pursuit AI (8 tickets)

### POLICE-001 — Create ChaseManager class
- Store heat level (0-5)
- Store police car to spawn
- **~15 min**

### POLICE-002 — Spawn a police car
- Create Vehicle with CreatePhysicsBody
- Place at an intersection near the player
- **~20 min**

### POLICE-003 — Basic pursuit AI
- Police car follows the player
- Uses `seekSteer` toward player position
- **~20 min**

### POLICE-004 — Heat level management
- Increases when player hits traffic cars
- Decreases over time
- **~15 min**

### POLICE-005 — Multiple police cars
- Spawn 1-5 cars based on heat level
- Each follows the player with an offset
- **~20 min**

### POLICE-006 — Deployment behavior
- Cars appear in front of the player (not behind)
- Use Paths to find positions
- **~20 min**

### POLICE-007 — Arrest
- If a police car touches the player → BUSTED!
- Display "BUSTED!" → MissionFailed
- **~15 min**

### POLICE-008 — Evasion
- If player gets far enough from cops → heat drops
- Heat at 0 → EVADED! → pursuit ends
- **~15 min**

---

## 3. UI Menus/Buttons (6 tickets)

### UI-001 — Mouse click detection on FrontendMultiSprite
- Read mouse position via SDL
- Compare with loaded sprites
- Log clicked sprite name
- **~20 min**

### UI-002 — Navigation callbacks
- Map sprite name to ScreenName
- On click, change screen (LIFO stack)
- **~15 min**

### UI-003 — Button visual states
- Normal: 100% opacity
- Hover: 80% opacity + highlight
- Pressed: 60% opacity
- Disabled: greyed out
- **~20 min**

### UI-004 — Text rendering (FrontendMultiText)
- Read MultiText data from P3D
- Draw text with TextureFont via SpriteBatch
- Handle horizontal/vertical alignment
- **~30 min**

### UI-005 — Main menu screen
- Load `BootScreen.p3d` → display buttons
- "New Game" → load level
- "Options" → options screen (placeholder)
- "Quit" → close game
- **~30 min**

### UI-006 — Pause menu in-game
- Display semi-transparent overlay
- "Resume", "Restart", "Main Menu", "Quit"
- Keyboard navigation (arrows + Enter)
- **~25 min**

---

## 4. Audio Streaming/Dialogue (5 tickets)

### AUDIO-001 — Double-buffer streaming
- Create 2 OpenAL buffers for music
- Fill buffer 1, play, fill buffer 2 while 1 plays
- Loop
- **~30 min**

### AUDIO-002 — Load a music RCF file
- Extract `music00.rcf` from CAB
- Load via `AudioManager::LoadRCF`
- Play via streaming
- **~20 min**

### AUDIO-003 — Prioritized dialogue system
- Queue: `std::deque<Dialogue>`
- Level 0 (cutscene) interrupts everything
- Level 1 (mission) interrupts level 2
- Level 2 (gameplay) plays if queue empty
- **~20 min**

### AUDIO-004 — End-of-sound callback
- Detect when an OpenAL source finishes
- Trigger next dialogue in queue
- **~20 min**

### AUDIO-005 — Synchronized subtitles
- Extract text from P3D Frontend bibles
- Display text at bottom of screen during sound duration
- **~25 min**

---

## 5. Post-FX / Shadows (5 tickets)

### FX-001 — Basic bloom
- Render scene to FBO
- Gaussian blur (2 passes)
- Add to original image
- **~30 min**

### FX-002 — Vignette
- Add dark edges overlay in post-process shader
- Configurable intensity
- **~10 min**

### FX-003 — Motion blur
- Save previous frame's viewProj matrix
- Reproject pixels in shader
- Blend with current image
- **~30 min**

### FX-004 — Basic shadow mapping
- Render scene from directional light into depth FBO
- Project depth texture in world shader
- Compare for shadow determination
- **~45 min**

### FX-005 — Color grading LUT
- Load 16×16×16 LUT texture
- Apply in post-process shader
- Adjust contrast/saturation
- **~20 min**

---

## Total: 30 micro-tickets

All achievable in 10-45 minute sessions.
Suggested priority: ASSET (unlocks real graphics) → POLICE (gameplay).
