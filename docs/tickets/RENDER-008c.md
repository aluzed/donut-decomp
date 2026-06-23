# RENDER-008c — LOD : swap de modèle selon la distance

- **Status:** TODO
- **Priority:** P2
- **Module:** Render
- **Depends on:** —
- **Files:** `src/Render/CompositeModel.cpp:123-131`, `src/Game.cpp:841-846`, `src/Level.cpp`

## Contexte
Chaque objet est rendu avec son mesh haute densité quelle que soit la distance à
la caméra. Les modèles SH&R fournissent souvent plusieurs niveaux de détail
(nommage `_lod0/_lod1/...` ou groupes distincts). Aucun mécanisme de sélection
n'existe dans `CompositeModel::Draw` (`CompositeModel.cpp:123-131`).

## Approche
1. Détecter/regrouper les variantes LOD au chargement : à partir des noms de
   props/meshes (`CompositeModel.cpp:97-105`), construire pour chaque objet une
   liste ordonnée de meshes par niveau de détail avec leurs distances de bascule.
2. Calculer la distance objet↔caméra par frame : position caméra disponible via
   `_camera` (`Game.cpp:841`), centre de l'objet via l'AABB (réutiliser
   RENDER-008a).
3. Dans la boucle de `CompositeModel::Draw` (`CompositeModel.cpp:126-130`),
   sélectionner l'index de LOD selon des seuils de distance configurables ; ne
   dessiner que le mesh choisi. Prévoir un LOD « impostor/aucun » au-delà d'une
   distance max.
4. Exposer les seuils LOD (et un override « forcer LOD0 ») dans l'overlay debug
   ImGui (`Game.cpp` ~l.1277).

## Critères d'acceptation
- [ ] Les objets lointains rendent un mesh de plus faible densité (vérifiable en
      wireframe).
- [ ] Les bascules sont stables (hystérésis ou seuils séparés) — pas de
      clignotement à la distance limite.
- [ ] Baisse du nombre de triangles soumis quand la caméra s'éloigne.
