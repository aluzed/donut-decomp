# RENDER-008a — Frustum culling by bounding box

- **Status:** DONE (2026-10-19)
- **Priority:** P2
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Core/Math/Frustum.h`, `src/Render/Mesh.*` (`GetBounds`,
  `MeshInstanced::GetInstancesBounds`), `src/Entity.h` (`GetBounds`),
  `src/Render/CompositeModel.cpp` (`Draw`), `src/Level.cpp` (`Draw`), `src/Game.cpp`
  (overlay)

## What was wrong

Every mesh of the level was drawn every frame, in view or not: some 3,150 mesh draws
on level 1 (both passes, opaque and translucent). The meshes' bounding box was declared
(`_boundingBoxMin/Max`) and never filled.

## What is done

- **`Frustum`** (`Core/Math/Frustum.h`): the six planes of a projection, taken from its
  matrix (Gribb and Hartmann: the fourth row plus or minus each of the others; the
  storage is column-major). A box is left out only when it lies wholly behind one plane.
  The near plane is OpenGL's (-w ≤ z), which also keeps everything a 0..w depth range
  sees.
- **Every mesh's bounds**, in its own space, from its vertices; a morph (`OffsetGroup`)
  grows them.
- **Static entities** stand in the world as they are: their mesh's box is tested against
  the camera's frustum.
- **Instanced meshes** (the trees, lamp posts, mailboxes of a zone) are one draw for all
  their instances: the box of all of them, in the world, is tested. It is recomputed after
  an instance moves (a prop knocked about); one gone (drawn at zero scale) is left out.
- **Composite models** (the level's animated objects, breakings, the wasps, the saucer):
  each prop is tested with the frustum of its whole transform (`viewProj * model *
  prop`), whose planes are in the mesh's own space, so its box is tested as it is.
- **Counted**: the camera overlay shows the meshes drawn and culled last frame
  (`CullStats`).

## Checked

- Level 1, the camera turning over the Simpsons' street: 1,800 to 2,600 of 3,150 mesh
  draws culled, 133 fps → 215.
- The same fixed views with and without the culling: the scenery is the same to the
  pixel; only the traffic and the pedestrians, which move between the two runs, differ.
  No popping.
- The seven stories under `--autopilot`, 240 s each: as many missions won as before, no
  failure, no crash.

## Not done

- The test is linear over every drawable (RENDER-008b would walk a spatial tree).
- Skinned models (characters, cars) and billboards are not tested; they are few.
- No occlusion culling: a house hides nothing behind it.
