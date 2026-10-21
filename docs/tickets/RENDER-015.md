# RENDER-015 — Lightmaps / éclairage statique (2e texture)

- **Status:** DONE (2026-10-21: closed, the data has none)
- **Priority:** P2
- **Module:** Render
- **Depends on:** RENDER-014
- **Files:** `assets/shaders/world.frag:3-9,:28-29`, `src/Render/Shader.cpp:18-22`, `src/Render/Mesh.cpp`, `src/Game.cpp:845-846`

## Contexte
Le `world.frag` échantillonne déjà une `lightmapTex` sur `lightmapUV` et la
multiplie au diffuse (`world.frag:28-29`), avec un `lightmapIntensity`. Mais la
texture n'est jamais **bind** sur l'unité dédiée ni le 2e canal UV garanti
(RENDER-014). Le `Shader` ne lit que la clé texture `"TEX"` (`Shader.cpp:18-22`),
pas la lightmap. Résultat : `lightmapTex` reste sur l'unité 0 (texture diffuse) →
aucun éclairage statique réel.

## Approche
1. Étendre `Donut::Shader` (`Shader.cpp:18-22`) pour reconnaître la clé texture
   lightmap (vérifier le nom de param P3D : `"TEX"` seconde occurrence, ou clé
   dédiée — grep les `TextureParams` réels). Stocker `_lightmapTextureName` +
   `SetLightmapTexture()` (parallèle de `SetDiffuseTexture`, `Shader.cpp:96-99`).
2. Dans `Shader::Bind` (`Shader.cpp:101-105`), binder la lightmap sur l'unité 1
   (`glActiveTexture(GL_TEXTURE1)`), diffuse restant sur l'unité 0.
3. Côté rendu monde (`Mesh::Draw` / `CompositeModel::Draw`), affecter les uniforms
   sampler `diffuseTex=0`, `lightmapTex=1` une fois, et exposer
   `lightmapIntensity`.
4. Résoudre les textures lightmap dans le `ResourceManager` lors du chargement
   (boucle textures `CompositeModel.cpp:83`). Fallback : texture blanche 1×1 si
   pas de lightmap (multiplication neutre).

## Critères d'acceptation
- [ ] `lightmapTex` est bind sur l'unité 1, distincte de `diffuseTex` (unité 0).
- [ ] Une zone avec lightmap montre un éclairage statique pré-calculé ;
      `lightmapIntensity=0` redonne le rendu non éclairé.
- [ ] Géométrie sans lightmap : rendu identique à avant (fallback blanc).

## Closed: the PC data has no lightmaps (2026-10-21)

Every UV list of every uncompressed P3D file was counted (`UVList`, 0x10007, its
channel in its second word): 19866 lists in 655 files, all channel 0. Not one second
channel, so nothing for a lightmap's coordinates. And every shader's texture parameter
(`0x11002`) was counted: 14053 `TEX`, 157 `REFL` (the cars' reflection maps), no other:
no shader has a second texture for one either. The game's world is lit by its vertex
colours, which are drawn.

`Mesh` no longer reads the second channel by catching the exception `GetUvs(1)`
throws: it asks `PrimitiveGroup::GetUvChannelCount()`. The world shader keeps its
lightmap sampler, at intensity 0.
