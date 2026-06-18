# Tickets — Gameplay

## GAME-001 — Refactor Game::Run (God Object)
**Priority:** P1
**File:** `src/Game.h`, `src/Game.cpp` (1000+ lines)
**Description:** The `Game` class is a huge God Object that does everything.
**Fix:** Extract into:
- `GameState` — global game state (menu, in-game, paused)
- `GameRenderer` — rendering pipeline
- `GameLoop` — clean main loop
- Keep `Game` as a thin orchestrator

## GAME-002 — 3-CD / file structure management
**Priority:** P1
**Description:** The original PC game is on 3 CDs. Implement virtual mounting or asset copying into an expected tree.

## GAME-003 — GameState machine (Menus → Game → Pause)
**Priority:** P1
**Description:** Game states:
1. **Splash/Intro** — Logos (Fox, Radical, Gracie Films) → RMV videos
2. **Main Menu** — Frontend loaded from BootScreen.p3d
3. **Level Select** — Level choice (7 levels)
4. **In-Game** — Gameplay
5. **Pause Menu** — Frontend pause
6. **Mission Complete/Fail** — Result screens
7. **Credits** — RMV + frontend

## GAME-004 — Player controls
**Priority:** P0 (blocking)
**Description:** Third-person character movement, jump, interact, enter/exit vehicle.

## GAME-005 — Vehicle system
**Priority:** P1
**Description:** Driving gameplay: vehicle model, physics, enter/exit, camera, nitro, damage, horn.

## GAME-006 — Game camera (third-person follow)
**Priority:** P1
**Description:** Follow cam behind character/vehicle, manual orbit, collision detection.

## GAME-007 — Traffic AI
**Priority:** P2
**Description:** Procedural traffic on roads. Road/RoadSegment/Intersection chunks define the road network.

## GAME-008 — Pedestrian AI
**Priority:** P2
**Description:** Pedestrians on sidewalks: wander, react to player, simple pathfinding.

## GAME-009 — Mission AI
**Priority:** P2
**Description:** Mission characters with scripted behaviors: follow, chase, flee, collect, attack.

## GAME-010 — Collectibles system
**Priority:** P2
**Description:** Coins, cards (7 per level), gags (~50 per level). Persistent state.

## GAME-011 — Save / Load
**Priority:** P2
**Description:** Save format: player state (position, vehicle, money, mission progress, collectibles).

## GAME-012 — Damage system
**Priority:** P3
**Description:** Visual damage on vehicles and breakable objects.

## GAME-013 — Complete Level loading (DynaLoad)
**Priority:** P1
**File:** `src/Level.cpp` (DynaLoadData)
**Description:** Entire DynaLoadData parsing is commented out. Dynamic zone loading system.

## GAME-014 — Interiors (Interior/Locator)
**Priority:** P1
**Description:** Interiors (Simpsons house, school, Kwik-E-Mart) loaded dynamically.

## GAME-015 — Debug UI (ImGui)
**Priority:** P3
**Description:** Keep ImGui debug UI but hide in release mode. Add toggle and integrated command console.
