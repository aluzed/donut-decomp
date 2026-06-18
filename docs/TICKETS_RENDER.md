# Tickets — Render

## RENDER-001 — Implement SkinAnimation::Track::Evaluate
**Priority:** P0 (blocking)
**File:** `src/Render/SkinAnimation.cpp:20`
**Description:** Entire body of `Track::Evaluate()` is commented out. Returns `Matrix4x4::Identity`. No skeletal animation works without this.
**Fix:** Implement evaluation loop: find keyframes, interpolate translation (lerp) and rotation (slerp/quat-lerp), compose matrix. Also implement `TranslationKey::Lerp` and `RotationKey::Lerp` (both return `b` as stubs).

## RENDER-002 — Fix LineRenderer::DrawBox (Matrix4x4)
**Priority:** P1
**File:** `src/Render/LineRenderer.cpp`
**Description:** All 12 `DrawLine` calls for box edges are commented out.

## RENDER-003 — Fix LineRenderer::DrawBox (Quaternion + Euler)
**Priority:** P1
**File:** `src/Render/LineRenderer.cpp`
**Description:** `DrawBox(Quaternion)` uses `Matrix4x4::Identity` instead of passed rotation. `DrawBox(Euler)` is entirely commented out.

## RENDER-004 — Reactivate SkinModel::UpdateAnimation
**Priority:** P1
**File:** `src/Render/SkinModel.h`
**Description:** `UpdateAnimation` is declared but commented out. Connect `Skeleton::UpdatePose()` and bone matrix upload to `TextureBuffer`.

## RENDER-005 — Verify skinning in vertex shader
**Priority:** P1
**Description:** Check that the vertex shader correctly reads bone indices/weights and applies matrices from the `TextureBuffer`.

## RENDER-006 — Implement CompositeModel_AnimObjectWrapper
**Priority:** P1
**File:** `src/Render/CompositeModel.h:22`
**Description:** Constructor leaves `_shaders` and `_textures` empty. `AnimObjectWrapper` requires shader/texture extraction.

## RENDER-007 — Implement CompositeDrawable methods
**Priority:** P2
**File:** `src/Render/CompositeModel.h:90`

## RENDER-008 — LOD and spatial culling
**Priority:** P2
**Description:** Add LOD system based on Tree chunks. Implement octree, frustum culling per zone, DynaLoad streaming.

## RENDER-009 — Post-processing pipeline
**Priority:** P3
**Description:** Motion blur, glow/bloom, heat haze, depth of field, iris wipe.

## RENDER-010 — Fog support
**Priority:** P2
**Description:** Fog is a P3D parameter (BillboardQuadGroup fog field + global params). Implement in world/instanced shaders.

## RENDER-011 — Billboards in WorldSphere
**Priority:** P2
**File:** `src/Render/WorldSphere.cpp`
**Description:** Billboard loading is commented out. WorldSpheres may contain billboards (lens flare, sun halo).

## RENDER-012 — Shadow mapping
**Priority:** P3
**Description:** Add basic shadow mapping (sun shadow map).

## RENDER-013 — Shader cache/precompilation
**Priority:** P3 (optimization)
**Description:** Shaders recompile on every launch. Add OpenGL binary cache or precompile variants.

## RENDER-014 — Multi-UV channel support
**Priority:** P2
**Description:** P3D primitives may have multiple UV channels. Channel 0 = diffuse, channel 1 = lightmap.

## RENDER-015 — Lightmaps and static lighting
**Priority:** P2
**Description:** Levels have pre-computed lightmaps. Shaders must sample a second texture for static lighting.
