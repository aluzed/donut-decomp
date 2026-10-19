# GAME-WASP — The wasp cameras

- **Status:** PARTIAL (2026-10-19)
- **Priority:** P2
- **Module:** Game
- **Files:** `src/Game/WaspManager.*`, `src/Game.cpp` (`loadLevel`, the update, the draw),
  `src/Render/CompositeModel.*` (`SetPropVisible`)

## What is done

Each level has twenty: the level script lists their spawn points
(`AddSpawnPointByLocatorScript("w_lemon", "beecamera", "Shelley", "w_lemon", ...)`), whose
locators are in `art/missions/levelNN/wasps.p3d` (now loaded with the level).

- **The model** is in the level's effects file (`art/lNN_fx.p3d`): the `beecamera` drawable,
  its skeleton, meshes and pose animation (`PTRN_beecamera`, 436 frames), taken apart from
  the file's other objects; only the shaders its meshes use are loaded.
- **Its states** are a state prop (`StaticPropData`, 0x8020000: version, name, factory,
  state count). Each state (0x8020001: name, auto-transition, next state, counts, out
  frame) holds per drawable its visibility (0x8020002: name, visible) and per frame
  controller (0x8020003: name, cyclic, cycles, hold frame, first frame, last frame, speed)
  its stretch of the animation, plus events (0x8020004, `DESTROYED`) and callbacks
  (0x8020005: `FIRE_ENERGY_BOLT`, `SPAWN_20_COINS`, `REMOVE_FROM_WORLD`...). For the wasp:
  0 coming in (0-75), 1 hovering (75-155, round and round), 2 and 3 alerted (155-315), 4
  firing (315-375), 5 destroyed (375-435, the head swapped for its exploding one).
- **Behaviour**: hovering at its spot, bobbing; turning to watch a player within 20m,
  alerted within 10m; destroyed by a jump at it (within 2.5m, in the second after the
  jump: being off the ground after a teleport is not one) or a car driven into it (3.5m,
  over 10 km/h). Destroyed, it plays its explosion and is gone, for 20 coins and the count,
  "Wasp cameras 1/20"; destroyed wasps stay so (`donut_wasps.dat`, every level's).

- **Their behaviours**, from the level script (`AddBehaviour`, for the class `beecamera` or
  one spawn point by name, which starts from the class's): `ATTRACTION` (nearest,
  farthest, speed in km/h; -1 stays put) draws a wasp to a player between the two, never
  more than 25m from its spot, and back; `ATTACK_PLAYER` (range, arc, period) fires once a
  period at a player in range within its arc (widened 10 degrees: it turns slower than a
  player walks): the firing state plays, and the shot costs `SetProjectileStats`'s damage
  (5 on level 1), with a zap and a jolt. It turns at `SetActorRotationSpeed`.

- **The shot** flies (`waspray`: two glowing billboards) from the wasp to where the player
  was, at `SetProjectileStats`'s speed (60 km/h on level 1), and costs its damage only if
  it reaches the player: one who keeps moving is missed. The wasp's own glows (its lens,
  its signal ring) are drawn where their joints are. Their shaders are loaded from the
  effects file; a billboard whose shader is missing is now skipped rather than crashing.
  The blending the billboards set is put back after: left additive, the frame washed white,
  left off, the HUD's text came out as boxes.

Checked: the wasp by the lemon tree hovering, its lens glowing; the one by the bonus car
destroyed by a jump, coming apart, 20 coins (60 to 80), "Wasp cameras 1/20"; twenty on
each of the seven levels; the bonus car's wasp coming from 6m to 4m and firing ten
seconds apart (level 1's period), health 100 to 95 to 90; the shot seen leaving it, a green
ball, and the health bar down as it lands.

## Not done

- `EVADE_PLAYER`.
- Their billboards and particle effects (lens glow, shockwave, `wasp_psShape`), the buzz.
- Wasps high up are out of a jump's reach: the game expects them reached by climbing.
