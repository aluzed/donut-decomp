# Tickets — UI

## UI-001 — Frontend: load all pages (not just "MessageBox.pag")
**Priority:** P1
**File:** `src/FrontendProject.cpp:82`
**Description:** `LoadP3D` only loads pages named "MessageBox.pag". Main menu, pause, options use other page names.
**Fix:** Load all pages (or by requested name).

## UI-002 — Frontend: implement screen system
**Priority:** P1
**Description:** The original game has a screen system (FrontendScreen → FrontendPage → FrontendLayer → FrontendGroup). Implement screen navigation:
Bootscreen → Main Menu → Level Select → Pause → Options → Credits

## UI-003 — Frontend: user interaction
**Priority:** P1
**Description:** FrontendObject/FrontendGroup have clickable zones. Implement:
- Mouse picking on sprites
- State changes (hover, pressed, disabled)
- Navigation callbacks

## UI-004 — Frontend text rendering (MultiText)
**Priority:** P1
**File:** `src/FrontendProject.cpp`
**Description:** `FrontendMultiText` is read but not rendered. Implement text rendering with `TextureFont` and `FrontendStringTextBible` for localization.

## UI-005 — In-game HUD
**Priority:** P1
**Description:** HUD elements:
- [ ] Health bar (hit points)
- [ ] Hit & Run gauge (police pursuit)
- [ ] Speedometer
- [ ] Coin counter
- [ ] Card counter
- [ ] Mission timer
- [ ] Current objectives
- [ ] Radar / mini-map
- [ ] Nitro indicator

## UI-006 — Pause menu
**Priority:** P2
**Description:** Pause overlay with options (resume, restart, quit, audio/video options).

## UI-007 — Localized text (bible system)
**Priority:** P2
**Description:** The game supports multiple languages via text bibles. Load the configured language bible.

## UI-008 — Loading screen
**Priority:** P3
**Description:** Screen with progress bar between levels/zones. Display hint text.

## UI-009 — Cutscenes (RMV player)
**Priority:** P3
**Description:** Game videos are in RMV (Radical Movie) format. Reverse or find existing codec to play FMVs.
