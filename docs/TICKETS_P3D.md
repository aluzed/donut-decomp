# Tickets — P3D

> P3D chunks are defined via `dev/codegen/p3d.json` → generated in C++ by `donut-codegen.exe`
> Adding a chunk = modify p3d.json + re-run codegen + implement the class

## P3D-001 — Add Particle System chunks (8 types)
**Priority:** P1
**Chunks:** `ParticleSystemFactory`, `ParticleSystem`, `BaseEmitter`, `SpriteEmitter`, `ParticleAnimation`, `EmitterAnimation`, `GeneratorAnimation`, `ParticleInstancingInfo`

Used for smoke, fire, car debris, nitro effects.

## P3D-002 — Add Light chunks (4 types)
**Priority:** P2
**Chunks:** `Light`, `LightDirection`, `LightPosition`, `LightShadow`, `LightGroup`

Static/dynamic level lighting. The original game has pre-computed lightmaps and dynamic lights (car headlights).

## P3D-003 — Add Expression/Facial Animation chunks
**Priority:** P3
**Chunks:** `Expression`, `ExpressionGroup`, `ExpressionMixer`, `ExpressionOffsets`

Character facial animations (dialogue).

## P3D-004 — Add Animation Controller chunks (8 types)
**Priority:** P2
**Chunks:** `FrameController`, `FrameController2`, `MultiController2`, `VertexAnimKeyFrame`, `VectorOffsetList`, `Float1Channel`, `Float2Channel`, `Vector1Channel`

Additional animation channels (float, int, bool, entity).

## P3D-005 — Add missing Frontend chunks
**Priority:** P2
**Chunks:** `FrontendStringHardCoded`, `FrontendTextBible`, `FrontendLanguage`, `FrontendObjectResource`, `FrontendTextStyleResource`, `FrontendTextBibleResource`

Localized text resources and translation bibles.

## P3D-006 — Add missing Entity chunks
**Priority:** P1
**Chunks:** `InstancedStaticEntity`, `Anim`, `AnimDynamicPhysicsWrapper`, `AnimCollision`

Entity variants not yet supported.

## P3D-007 — Add Tree chunks (spatial)
**Priority:** P2
**Chunks:** `Tree`, `TreeNode`, `TreeNode2`

Spatial structure (likely octree/kd-tree) for culling and spatial queries.

## P3D-008 — Add missing Route/Level chunks
**Priority:** P1
**Chunks:** `Spline`, `LocatorMatrix`, `SurfaceTypeList`

Splines for camera paths, locator matrices, surface types for physics (road, grass, sidewalk).

## P3D-009 — Add missing Collision chunks
**Priority:** P1
**Chunks:** `CollisionWallVolume`, `CollisionEffect`, `Atc`

## P3D-010 — Add Static Prop chunks
**Priority:** P2
**Chunks:** `StaticPropData`, `StaticPropStateData`, `StaticPropVisibilityData`, `StaticPropFrameControllerData`, `StaticPropEventData`, `StaticPropCallbackData`

## P3D-011 — Add missing Geometry chunks
**Priority:** P2
**Chunks:** `Mesh`, `OffsetList`, `PackedNormalList`, `VertexShader`, `DrawShadow`

## P3D-012 — Add Export Info chunks
**Priority:** P3
**Chunks:** `ExportInfo`, `ExportInfoNamedString`, `ExportInfoNamedInteger`

3ds Max → P3D export metadata.

## P3D-013 — Implement P3DUtil::GetShaderTexture()
**Priority:** P0 (blocking)
**File:** `src/P3D/P3DChunk.h:267` (declared, never defined)
**Description:** Utility function declared but never implemented. Either implement or remove.

## P3D-014 — Implement CompositeDrawableSkin/Effect parsing
**Priority:** P2
**File:** `src/P3D/P3D.generated.cpp`

## P3D-015 — Document `_todo` fields in P3D code
**Priority:** P3 (research)
**Description:** Fields read but purpose unknown. Reverse via Ghidra.

## P3D-016 — Reverse P3DZ format (proprietary compression)
**Priority:** P2
**Description:** Compressed format used for many game files. Header is `0x5A443350`. Reverse via Ghidra.

## P3D-017 — Support non-PNG images
**Priority:** P3
**File:** `src/P3D/P3DChunk.cpp` (ImageDecoder::Decode)
**Description:** Only PNG supported. P3D images may use other formats (palettized, DXT).
