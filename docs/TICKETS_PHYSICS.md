# Tickets — Physics

## PHYS-001 — Reactivate stepSimulation
**Priority:** P0 (blocking)
**File:** `src/Physics/WorldPhysics.cpp` (Update)
**Description:** `mDynamicsWorld->stepSimulation(dt)` is commented out. Physics world exists but simulates nothing.
**Fix:** Uncomment `stepSimulation`. Verify dt is correct (fixed or variable).

## PHYS-002 — Complete CharacterController
**Priority:** P0 (blocking)
**File:** `src/CharacterController.cpp`
- [ ] Implement `canJump()` (ground detection via raycast)
- [ ] Implement `jump()` (vertical velocity)
- [ ] Implement `onGround()`
- [ ] Implement `debugDraw()` (controller debug render)
- [ ] Implement `reset()` (reinitialize)
- [ ] Implement `setVelocityForTimeInterval()` (impulse)
- [ ] Fix destructor (commented-out Bullet cleanup)
- [ ] Fix gravity value (likely ~9.8 m/s², not 1.0)
- [ ] Fix `convexSweepTest` (commented "this doesn't work TWAT")

## PHYS-003 — Integrate static collisions into the world
**Priority:** P1
**File:** `src/Level.cpp` (LoadP3D)
**Description:** Calls to AddCollisionVolume, AddP3DOBBoxVolume, AddP3DFence, AddIntersect are commented out.
**Fix:** Uncomment and validate each volume type conversion to Bullet.

## PHYS-004 — Fix collision rotations
**Priority:** P1
**File:** `src/Physics/WorldPhysics.cpp`
**Description:** Several TODOs for Fence rotation, Cylinder rotation, transforms.

## PHYS-005 — Use BulletFenceShape instead of btBoxShape
**Priority:** P2
**Files:** `src/Physics/WorldPhysics.cpp` (AddP3DFence), `src/Physics/BulletFenceShape.cpp`
**Description:** AddP3DFence creates btBoxShape instead of using the custom BulletFenceShape.

## PHYS-006 — Evaluate Bullet's btKinematicCharacterController
**Priority:** P3 (alternative)
**Description:** Consider whether Bullet's built-in btKinematicCharacterController can replace the custom one.

## PHYS-007 — Vehicle physics (raycast vehicle)
**Priority:** P1
**Description:** Implement btRaycastVehicle for cars. The original game uses arcade vehicles with:
- Ray suspensions
- Engine with gearbox
- Braking, drift, nitro
- `dev/Commands.md` lists ~30 vehicle tuning parameters

## PHYS-008 — Surface types (SurfaceTypeList)
**Priority:** P2
**Description:** Different physics properties per surface (road, grass, sidewalk, sand).

## PHYS-009 — Fence collisions
**Priority:** P1
**Description:** Fences are invisible collision walls around levels.

## PHYS-010 — Intersect mesh collisions
**Priority:** P1
**Description:** Intersect meshes are optimized low-poly collision meshes.
