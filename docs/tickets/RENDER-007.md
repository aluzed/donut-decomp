# RENDER-007 — Implémenter la classe `CompositeDrawable`

- **Status:** TODO
- **Priority:** P2
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Render/CompositeModel.h:96-105`, `src/Render/CompositeModel.cpp`

## Contexte
La classe `CompositeDrawable` (`CompositeModel.h:96-105`) n'est qu'une
déclaration : un constructeur `CompositeDrawable(const P3D::CompositeDrawable&)`
sans corps et deux vecteurs `_meshes` / `_billboards`. Aucun `.cpp` ne la définit.
Aujourd'hui le tri skins/props/effects est aplati directement dans
`CompositeModel::CompositeModel` sous forme de `DrawableProp`
(`CompositeModel.cpp:85-106`), ce qui empêche un rendu par drawable (joint,
opacité, billboards séparés).

## Approche
1. Décider du rôle : soit finaliser `CompositeDrawable` comme unité de rendu
   (un drawable = un ensemble props/skins/billboards lié à un `Skeleton`), soit
   supprimer la classe morte si `DrawableProp` suffit. Le commentaire
   `CompositeModel.h:91-95` documente l'intention (skins/props/effects triés).
2. Implémenter dans `CompositeModel.cpp` le constructeur : parcourir
   `P3D::CompositeDrawable::GetPropList()->GetProps()` (cf. usage existant
   `CompositeModel.cpp:87-91`), résoudre les meshes via les noms, remplir
   `_meshes` ; brancher les billboards via `BillboardBatch`
   (cf. `WorldSphere.cpp:24-28`).
3. Ajouter une méthode `Draw(GL::ShaderProgram&, const Matrix4x4& viewProj, bool
   opaque)` qui itère `_meshes`/`_billboards` (modèle : `WorldSphere::Draw`
   `WorldSphere.cpp:143-156`).
4. Refactorer `CompositeModel` pour stocker des `CompositeDrawable` au lieu de
   `_props`, ou laisser les deux chemins coexister selon la décision en (1).

## Critères d'acceptation
- [ ] `CompositeDrawable` se compile et se construit depuis un
      `P3D::CompositeDrawable` réel.
- [ ] `Draw` rend meshes et billboards d'un drawable au bon joint du squelette.
- [ ] Aucun symbole déclaré-non-défini restant (link OK).
