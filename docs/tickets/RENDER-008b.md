# RENDER-008b — Octree / kd-tree from the `Tree` chunks

- **Status:** CLOSED (2026-10-21): built, measured, slower than the linear test; not kept
- **Priority:** P2
- **Module:** Render
- **Depends on:** P3D-007, RENDER-008a
- **Files:** `src/Level.cpp` (`Level::Draw`), `src/P3D/SpatialTree.*`

## Context
RENDER-008a culls each drawable against the view frustum, one by one. The levels carry a
kd-tree (`Tree`/`TreeNode`, read by P3D-007) that would let whole regions be left out at
once.

## Tried (2026-10-21)
The static entities were placed in the level's tree as the game places its objects, each
in the deepest node whose split it does not straddle, and each node kept the box of all its
subtree holds, so that a node out of sight was left out with everything under it (exact:
a box that passes the frustum test is inside every box above it, which pass too). The
placement agrees with the file: in 2932 of level 1's 3103 nodes (1837 of level 3's 2111) we
placed as many static entities as the file counts there.

Measured over 60 s of play, 600-frame averages, against the linear test on the same frames:

| | static entities | in sight | tree | linear | mismatches |
|---|---|---|---|---|---|
| Level 1 | 949 | 270-326 | 44-68 µs | 30-44 µs | 0 |
| Level 3 | 827 | 323-361 | 42-48 µs | 24-28 µs | 0 |

The same entities in sight, and **the tree slower by half**. A level holds under a
thousand static entities, and a third of them are in sight from the street: walking 3103
nodes to skip two thirds costs more than testing them all, which takes 30-40 µs a frame
(0.2% of 16 ms). The code was not kept.

## Acceptance criteria
- [x] A spatial index built at load (the level's own tree): done and measured, then removed.
- [x] The tree's query returns the same visible set as the linear test (0 mismatches).
- [ ] A measurable gain on culling time: **none**, it is slower at this scene size.
  Worth trying again only if the culling ever shows in a profile.
