# PHYS-005 — Utiliser BulletFenceShape au lieu de btBoxShape dans AddP3DFence

- **Status:** DONE
- **Priority:** P2
- **Module:** Physics
- **Depends on:** —
- **Files:** `src/Physics/WorldPhysics.cpp:128`, `src/Physics/WorldPhysics.cpp:202-220`, `src/Physics/BulletFenceShape.cpp`, `src/Physics/BulletFenceShape.h`

## Contexte
`AddP3DFence` (`WorldPhysics.cpp:202-220`) construit déjà correctement un `BulletFenceShape` à partir de `start/end/normal`. En revanche `AddIntersect` (`WorldPhysics.cpp:128`) utilise encore un `btBoxShape` générique. Le but est d'unifier la représentation des barrières sur la forme dédiée `BulletFenceShape` (collision plane orientée par la normale) et de vérifier que `processAllTriangles` produit la bonne géométrie.

## Approche
1. Auditer l'usage de `btBoxShape` (`WorldPhysics.cpp:128`) et déterminer les cas qui correspondent en réalité à des barrières → les router vers `BulletFenceShape`.
2. Vérifier l'implémentation de `BulletFenceShape::processAllTriangles` / `getAabb` (`BulletFenceShape.cpp`) : AABB correcte couvrant `m_start`→`m_end`, deux triangles formant le quad, normale respectée.
3. Confirmer que `AddP3DFence` reste correct (la forme et la `worldTransform` à l'origine `start`).
4. Tester en collision contre une barrière (véhicule + personnage) que la réponse suit le plan et non une boîte.

## Critères d'acceptation
- [x] Les barrières utilisent `BulletFenceShape`, plus de `btBoxShape` pour ce cas.
- [x] `processAllTriangles` génère le quad orienté par `m_normal` (vérifié en debug draw).
- [x] La collision véhicule/personnage contre une barrière reste solide (pas de traversée).
- [x] Pas de régression sur `AddIntersect` pour les volumes réellement boîtes.
