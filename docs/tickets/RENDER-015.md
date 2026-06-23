# RENDER-015 — Lightmaps / éclairage statique (2e texture)

- **Status:** TODO
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
