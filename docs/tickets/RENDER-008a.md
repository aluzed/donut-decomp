# RENDER-008a — Frustum culling par AABB de drawable

- **Status:** TODO
- **Priority:** P2
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Render/Mesh.h:57-58`, `src/Render/CompositeModel.cpp:123-131`, `src/Game.cpp:841-846`, `src/Level.cpp`

## Contexte
Tous les meshes du niveau sont dessinés inconditionnellement (`Level::Draw` via
`CompositeModel::Draw`, `CompositeModel.cpp:123-131`), sans test de visibilité. Le
`Mesh` calcule pourtant déjà `_boundingBoxMin` / `_boundingBoxMax`
(`Mesh.h:57-58`). On peut donc rejeter par frustum les objets hors champ.

## Approche
1. Construire les 6 plans du frustum à partir de la `viewProjection`
   (`Game.cpp:841-843`) : extraction Gribb-Hartmann des plans depuis la matrice
   combinée. Ajouter un petit helper `Frustum` (Core/Math) ou une fonction libre.
2. Exposer l'AABB des meshes : ajouter des accesseurs `GetBoundsMin/Max` sur
   `Mesh` (`Mesh.h:57-58`), et propager une AABB monde par `DrawableProp`
   (transformer l'AABB locale par `transform`, `CompositeModel.cpp:104`).
3. Dans la boucle de `CompositeModel::Draw` (`CompositeModel.cpp:126-130`),
   tester `frustum.Intersects(aabbMonde)` ; sauter le `Draw` si rejeté.
4. Ajouter un compteur de draw calls (rendus vs cullés) affiché dans l'overlay
   ImGui debug (`Game.cpp` zone debug ~l.1277) pour mesurer l'effet.

## Critères d'acceptation
- [ ] Les objets hors du frustum ne sont pas dessinés (vérifiable visuellement +
      compteur).
- [ ] Le compteur de draw calls baisse quand on regarde une zone vide / on pivote.
- [ ] Aucun « popping » d'objets pourtant visibles (AABB transformée correctement).
