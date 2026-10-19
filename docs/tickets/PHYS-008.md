# PHYS-008 — Grip per surface (road, grass, sand, ...)

- **Status:** DONE (2026-10-20)
- **Priority:** P2
- **Module:** Physics
- **Depends on:** —
- **Files:** `src/P3D/P3D.generated.*` (`Intersect::GetSurfaceTypes`),
  `src/Physics/WorldPhysics.*` (`Surface`, `GetSurfaceUnder`), `src/Vehicle.*`

## What was wrong

The tyres held the same everywhere: one friction figure for every wheel, on the road as
on the grass or the beach's sand.

## What is done

- **The ground's surface**: each `Intersect` (0x3F00003, the level's ground meshes)
  carries a `SurfaceTypeList` (0x300000E: a version, a count, then a byte a triangle).
  The byte is the game's terrain type: 0 road (and pavement), 1 grass, 2 sand, 3 gravel,
  4 water, 5 wood, 6 metal, 7 dirt; 0x80 is set on an interior's. Read off the levels:
  level 1's and 4's are road, grass, dirt and a little sand; level 3's and 6's (the
  Squidport) have their boardwalks of wood and their beach of sand; level 2's and 5's
  some metal and gravel.
- **What is under a point** (`WorldPhysics::GetSurfaceUnder`): a short ray down to the
  level's ground, the triangle it meets, its byte. Road where nothing says otherwise.
- **Each wheel's grip**: the ground under each wheel in contact is looked up every frame
  and sets that wheel's friction (`btWheelInfo::m_frictionSlip`) for the next step, out
  of the road's: grass and dirt 0.75, gravel 0.7, sand and water 0.6, metal 0.9, wood as
  the road. These factors are ours; the game's own are not known.
- **A wheel's contact between physics steps**: `updateWheelTransform`, called every
  drawn frame, clears it, and only the next step finds it again. Drawn at 240 frames a
  second over 60 steps, the car touched nothing three frames in four: it was "airborne"
  (the driver's jump line counted from it, the last safe place to put the car back was
  kept one frame in four). The contact is now kept until the next step.
- **Skid marks** (`Render/SkidMarks`): a wheel braking hard over 30 km/h, or going
  sideways over the ground faster than 3.5 m/s, leaves a strip of quads behind it, 30 cm
  wide, laid a few centimetres above the ground, a quad every 40 cm; with the effects
  file's shader for the ground under it: `skid_m` (black) on the road, wood and metal,
  `grass_skidmark_shader` on grass, `dirt_skidmark_shader` on dirt, sand, gravel, none on
  water. 1,024 quads a shader are kept, the oldest go first.
- **What the wheels throw up**: the level's effects file holds the game's own wheel
  sprays, by particle type: 11 grass (`spray_grass_psShape`: blades and clumps of the
  level's grass), 10 earth and stones (`dirt_sprayPSShape`). A car's rear wheels, over
  15 km/h, give off the grass one on grass and the earth one on dirt, sand and gravel, at
  the wheel's contact, tipped back from straight up so it flies behind the car.

## Checked

- Homer's car at 50 km/h braking hard along the pavement by a grass strip: a black mark
  on the pavement, a green one on the grass, ending as it slows under 30 km/h.
- Homer's car across the Simpsons' lawn at 50 km/h: grass flies up behind it, and stops
  as it leaves the lawn for the drive.
- Level 3 under `--autopilot`: Lisa's car starts on the boardwalk (wood), an opponent
  cuts across grass and back onto the road; the surface under each wheel follows.
- The seven stories under `--autopilot`, 240 s each: as many missions won as before, no
  failure, no crash.

## Not done

- `SetTireGrip`, the car scripts' own figure (1.4 to 1.95, one at 15), is still not
  applied: taken as it is, it would change every car's handling.
- No tyre sound per surface yet (the surface is there for it:
  `Vehicle::GetWheelSurface`).
- The cars never slide sideways for long: their tyres hold far too well (a wheel's
  sideways speed goes over 2 m/s for one frame at a time), there is no handbrake to
  drift on, and the AI lifts off rather than brakes. So marks come mostly from the
  player braking.
- Effect types 9 (`dashShape`, the dust of the player's dash), 14 (`car_damage3SysShape`,
  a damaged car's smoke and fire) and 19 (`FireSpraySysShape`) are now named, not used.
  A wrecked car's smoke was tried: the smoke rises from its hood well, its fire is a
  white glare (see P3D-001, what `NUM` is).
