# VEH-WHEELS — Cars stand on their own wheels

- **Status:** DONE (2026-10-21)
- **Priority:** P1 (reported in play: "the wheels are half under the road")
- **Module:** Physics / Vehicle / AI
- **Files:** `src/Vehicle.{h,cpp}` (`LoadModel`, `CreatePhysicsBody`, `rideHeight`,
  `SetPosition`), `src/Render/CompositeModel.{h,cpp}` (`SetJointRotation` with an
  offset, `GetJointPropRadius`, `GetRestBottom`), `src/AI/TrafficManager.{h,cpp}`,
  `src/Physics/WorldPhysics.{h,cpp}` (`CreateBoxesShape`), `src/Game.cpp`
  (`updateInterior`)

## Context
Cars were often drawn sunk into the road, their wheels half below it. Three causes:

1. **One set of physics wheels for every car.** `CreatePhysicsBody` hung four wheels
   0.8 m out and 1.8 m along, at the chassis' middle, 0.4 m in radius, whatever the
   model. A model whose hubs sit 0.3 m lower than its middle, or whose wheels are
   0.8 m across, was held at the height of the generic wheels: its own sank into the
   road (or, for a small one, stood off it).
2. **The drawn wheels ignored the suspension.** The wheel joints only turned (steering,
   rolling); the physics' wheels moved up and down with their springs.
3. **Traffic rode at one height for all.** `TrafficManager` put every model's middle
   0.7 m above the path, a pickup's and a van's wheels in the ground.

## Fix
- `Vehicle::LoadModel` records each wheel joint's rest hub and its wheel's radius (half
  the height of the props skinned to that joint). When the four are found, the
  physics' wheels hang from just above each hub, `suspensionRestLength` up, with that
  radius: at rest, the physics' wheel is where the model's is.
- `Vehicle::SetPosition` moves each wheel joint up by its suspension's compression
  (`restLength - suspensionLength`, clamped to ±0.3 m).
- A car is spawned or teleported at `rideHeight()`: its lowest wheel bottom on the
  ground plus 0.3 m to settle, never less than the old 0.9 m.
- `TrafficManager` keeps a ride height per model, from its wheels (hub less radius)
  or, without wheel joints, its lowest point.
- The chassis is the car's own collision object (the `CollisionObject` named as the
  car, in each of the 88 car files): the boxes on its root joint (a volume's object
  reference is the joint it follows) as one compound shape. Its other volumes are on
  their own joints, in their space: the wheels' spheres (joints 20-23), which the
  raycast wheels stand for, and the doors' boxes. Taken in the car's space, as a first
  try did, `cPolice`'s door boxes reached 0.1 m below its wheels and the police car,
  dragged on them, never moved: levels 4 and 5 won 1 mission each instead of 3 and 5.
  A chassis reaching below its wheels falls back to the one 1.8 x 0.8 x 4.4 m box,
  as does a file without a collision object. The cars run from a 1.5 m dune buggy to
  a 10 m fire truck; most bodies' bottoms are 0.1-0.3 m above the old box's.

## Found on the way: the town gone around a conversation
Screenshots of level 2 showed Bart and Snake talking on an empty sky-blue floor. The
player had gone into Moe's, then been put in a car outside (the autopilot's `getin`),
never through the door: the interior's drawing-only (`SetDrawOnly`) stayed on, the
town undrawn. `Game::updateInterior` now leaves the interior when the player is in a
car or more than 80 m from its way out.

## Verification
- Measured in play (level 2, `honor_v`): each wheel in contact, hub to contact
  0.26 m = its radius, chassis at rest; `skinn_v` likewise (0.38 m).
- Measured models: `famil_v` hubs at -0.21 m, wheels 0.76 m across; `snake_v` hubs at
  -0.37/-0.30 m, wheels 0.70-0.82 m; `honor_v` -0.32 m, 0.52 m.
- `val.sh 240` and the mini-game run unchanged from the baseline.
