# GAME-WASP — The wasp cameras

- **Status:** DONE (2026-10-21: 137 of 140 in reach)
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

- **Evading** (`EVADE_PLAYER`: near, far, low, high, speed; levels 2 to 5 and 7): alerted,
  a wasp hops once a period (`ATTACK_PLAYER`'s third number) to a spot near..far metres
  from its own and low..high above it, at its speed, instead of following the player.
  Checked on level 2 from the log: the wasp by (299.5, 10.2, 32.7) dodging about it,
  0.7 to 1.2m higher, firing between hops.
- **Coming and going**: a wasp is out only while the player is within its spawn point's
  radius (`AddSpawnPointByLocatorScript`'s fifth argument, 8 to 60m): it comes in as they
  enter it (state 0, then hovering by itself: it drops in, spreads its wings, its signal
  ring lights) and goes, unharmed, once they are well away (1.5 times the radius, plus
  10m). Checked: absent, then dropping in by the bonus car as the player arrives.
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

- **Sounds** (`sound\soundfx\wasp\`), each where it happens: a wasp within 20m buzzes
  (`wasp_idle_03`, every 2s), a shot (`wasp_laser`), a hit (`character_zapped_02`) or a
  miss (`laser_miss`), a wasp coming apart (`wasp_destroyed`). Checked from the log, each
  found and decoded; not listened to.

- **Glows animated**: their billboards follow their `BQG_*` animations at the wasp's frame
  (size, colour, visibility): the signal ring pulses, the lens lights up once the wasp has
  come in, the shockwave grows and fades as it arrives. Checked: the ring absent, small,
  large, faded across four frames.
- **Particle effects**: the beecamera's effect list places three systems at its joints
  (`wasp_psShape`, `asdasdasdfShape`, `fadein_psShape`), each drawn when the current state
  shows it, keyed to its animation's frame: checked, a wasp blowing up gives a puff of smoke
  that spreads and fades, and its pieces fall.

Checked: the wasp by the lemon tree hovering, its lens glowing; the one by the bonus car
destroyed by a jump, coming apart, 20 coins (60 to 80), "Wasp cameras 1/20"; twenty on
each of the seven levels; the bonus car's wasp coming from 6m to 4m and firing ten
seconds apart (level 1's period), health 100 to 95 to 90; the shot seen leaving it, a green
ball, and the health bar down as it lands.

## Not done

- Wasps high up were out of a jump's reach: see below.

## Within reach (2026-10-21)

Each wasp's height over the ground under it was measured, on the seven levels: 129 of
140 float within a jump's reach (a jump rises 1.2 m, the strike reaches 2.5 m from the
player's middle: up to some 4.8 m). Of the eleven over it:
- four of level 4's (Burns' estate, the chessboard) floated over nothing: the estate is
  `l4r4b.p3d`, left out as the "after" version of `l4r4a` (level 1's pair is before
  and after a change), when level 4's street races load both. A road's second version
  is now loaded when the level's scripts load it: the estate is there, ground 1.4 to
  2 m under its wasps;
- the game's double jump was missing (the tutorial says it: "quand tu es dans les airs,
  réappuie sur la touche A pour sauter plus loin"): in the air, the jump pressed again
  goes up as high again (2.4 m in all), then a kick. Four more in reach from the roof
  or ledge under them (the gas station's, level 4's barn, level 6's bowl, level 7's
  plant lot): 0.8 to 1.8 m from a double jump's top.

Three are still out of reach, measured from every surface within 15 m (a running
jump's 2 m allowed): level 4's `w_powerplant2` (3.6 m short), level 5's `w_rail2`
(6.6 m), level 7's `w_barn` (4.7 m). Their behaviours keep them up (EVADE_PLAYER moves
them 1 to 2 m above their spot, ATTRACTION at its height).
