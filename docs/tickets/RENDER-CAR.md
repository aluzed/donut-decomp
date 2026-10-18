# RENDER-CAR — Cars drew as screen-wide garbage, or not at all

- **Status:** DONE (2026-10-18)
- **Priority:** P1
- **Module:** Render / Vehicle
- **Depends on:** —
- **Files:** `src/Game.cpp` (mission vehicle draw, follow cameras), `src/Level.h`

## What was seen

Running sr2 (`--mission .../sr2i.mfk`), the player's view at the start was half covered by
flat black, blue and grey polygons whose edges all ran out from **the exact centre of the
screen**. They changed from run to run. The car the player sat in was nowhere to be seen.
The M1race opponent had looked odd too (a red box with a teal sliver), but only ever from far
away.

Hiding every mission car made the screen clean. Hiding any single one did not, so it was every
car, not one bad model.

## Cause

`Game` drew each mission car with the **skinning shader** (`skin.vert`), in the translucent
pass only (`opaque = false`).

A car is a `CompositeModel` of plain P3D `Mesh`es, whose vertex layout is position, uv,
lightmap uv and colour. That is `world.vert`'s layout. `skin.vert` reads a normal, bone weights
and bone indices at the same attribute slots, gets uv and colour data instead, and produces
degenerate clip-space positions: triangles with a vertex at w ~ 0, which is why the debris
fanned out from the screen centre. And with only the translucent pass drawn, the opaque body
of the car, which is nearly all of it, was never drawn at all.

## Fix

Cars are drawn the way the level draws its own composite props: with the level's world shader
(`Level::GetWorldShader`), opaque pass then translucent pass. Checked from the same camera on
M1race: `snake_v` and `famil_v` now render as the textured cars they are, sitting on the road.

## Also: the follow cameras keep the subject in view

Ruled out while chasing the above, but worth having: both follow cameras (car and on-foot)
now cast a ray, against static geometry only, from what they look at to where they want to
be. If the level is in between, they move in to just this side of it, instead of sitting
inside a hedge.

## Acceptance criteria

- [x] No screen-centre debris with mission cars present (sr2 start, compared with and
      without cars).
- [x] A car seen from close range renders whole and textured (M1race, `snake_v` and
      `famil_v`).
- [x] The player's car is seen from the chase camera at sr2's grid (see "The view that
      turned the wrong way" below).

## The yaw that was in degrees (2026-10-18, later)

`Quaternion::Euler()` returns **degrees**, and folds any heading past 90 degrees back into
[-90, 90] with X and Z flipped to 180. Three places took its Y as a yaw in radians: the
chase camera (offset and look direction), the on-foot turn toward the walking direction,
and the traffic's steering. A car heading 0.5 rad gave the camera a yaw of 28.6 rad. They
now use `Quaternion::Yaw()`, the heading of +Z by `atan2`, checked against `MakeFromEuler`
for 0, 0.5, pi/2, 2.5 and -2.5.

The chase camera also looks **at the car** from wherever it ends up, rather than along the
car's heading at a fixed pitch, so a camera pulled in by the level still frames the car.

## The view that turned the wrong way (2026-10-18, solved)

The sr2 grid was not a hidden car but a camera looking the other way. `FreeCamera` keeps
the **view's** rotation (world to camera), the inverse of the camera's own: its own
`Move` takes `_orientation.Inverse()` to go back to the world. Both follow cameras gave it
the camera's own rotation, so the view turned the other way about Y. For a heading of 0
or pi the two are the same, which is why every earlier check looked right; for any other
the camera stood in the right place and looked away from its subject. sr2's grid faces +X,
so the camera sat behind the car and showed the lawn behind itself. On foot, turned a
quarter, it showed the street with Homer out of the frame.

`Game::cameraLooking(pitchDown, yaw)` now builds the view rotation, inverse included, for
both. Checked: sr2's grid shows `famil_v` square in the frame with the school bus ahead;
on foot at a quarter turn, Homer is in the middle. `--autopilot` story runs as before.

- [x] The player's car is seen from the chase camera at sr2's grid.
