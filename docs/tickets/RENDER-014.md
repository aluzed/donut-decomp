# RENDER-014 — Support multi-canal UV (canal 1 = lightmap)

- **Status:** DONE (2026-10-21: closed, the data has none)
- **Priority:** P2
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Render/Mesh.cpp:47-53`, `src/Render/Mesh.h:38-43`, `assets/shaders/world.vert`, `assets/shaders/world.frag:14`

## Contexte
La géométrie SH&R peut porter deux jeux d'UV : canal 0 (diffuse) et canal 1
(lightmap). Le `Vertex` de `Mesh` a déjà `uv` **et** `lightmapUV`
(`Mesh.h:38-43`), et `Mesh.cpp:47-53` lit `GetUvs(0)` puis tente `GetUvs(1)` dans
un `try/catch` silencieux. Le `world.frag` consomme déjà `lightmapUV`
(`world.frag:14`). Il faut fiabiliser l'extraction du 2e canal et son passage au
vertex layout / VAO, sans le `catch(...)` muet. Ce ticket débloque RENDER-015.

## Approche
1. Dans `Mesh.cpp:47-53`, remplacer le `try { ... } catch (...) {}` par une
   détection propre : interroger le nombre de canaux UV du `P3D::PrimGroup`
   (flags / `GetNumUVChannels` — vérifier l'API dans `P3D.generated.h`) avant
   d'appeler `GetUvs(1)`. Si absent, remplir `lightmapUV` avec `uv` (canal 0) ou
   `(0,0)` selon le choix de fallback.
2. Vérifier que `CreateVertexBinding` (`Mesh.cpp`) déclare bien un
   `GL::ArrayElement` pour `offsetof(Vertex, lightmapUV)` sur un location dédié
   (cf. layout de `SkinModel.cpp:74-80` pour le modèle).
3. S'assurer que `world.vert` déclare l'attribut `lightmapUV` au bon location et le
   transmet dans `VertexData.lightmapUV` (consommé par `world.frag:14`).
4. Étendre `BillboardBatch`/`SkinModel` plus tard si nécessaire (hors scope ici :
   focus géométrie statique du monde).

## Critères d'acceptation
- [ ] Plus de `catch(...)` muet ; détection explicite de la présence du canal 1.
- [ ] Une géométrie sans canal 1 charge sans erreur (fallback déterministe).
- [ ] Le canal 1 atteint bien `world.frag` (vérifiable en affichant `lightmapUV`
      comme couleur de debug).

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
