# PHYS-011 — Corps rigides dynamiques pour AnimDynamicPhysics

- **Status:** TODO
- **Priority:** P1
- **Module:** Physics
- **Depends on:** —
- **Files:** `src/Level.cpp:149-169`, `src/Physics/WorldPhysics.cpp`, `src/Physics/WorldPhysics.h`

## Contexte
Les chunks `AnimDynamicPhysics` (`Level.cpp:149-169`) sont chargés et instanciés visuellement (`CompositeModel` poussé dans `_compositeModels`), mais aucun corps Bullet n'est créé : ces objets (lampadaires, poubelles, boîtes...) ne réagissent pas aux collisions et ne tombent pas. Related P3D-006.

## Approche
1. Dans le `case P3D::ChunkType::AnimDynamicPhysics` de `Level.cpp:149-169`, récupérer la géométrie de collision via `AnimDynamicPhysics` (`GetInstanceList`, et les volumes physiques du wrapper).
2. Ajouter une méthode `WorldPhysics::AddDynamicBody(...)` (à côté de `AddIntersect`/`AddP3DFence` dans `WorldPhysics.cpp`) qui crée un `btRigidBody` dynamique : shape approchée (`btBoxShape`/`btConvexHullShape`), masse non nulle, `calculateLocalInertia`, `btDefaultMotionState` initialisé depuis `transform`.
3. Conserver le couplage modèle↔body afin que `CompositeModel::SetTransform` suive le `btMotionState` chaque frame (mise à jour dans `Level::Update`).
4. Stocker les bodies pour libération et pour `unloadRegion`.

## Critères d'acceptation
- [ ] Chaque `AnimDynamicPhysics` instancié possède un `btRigidBody` dynamique dans le monde.
- [ ] Le `CompositeModel` correspondant suit la transformation du `btMotionState` par frame.
- [ ] Les objets réagissent aux impacts du véhicule/personnage (poussée, chute).
- [ ] Les bodies sont libérés au déchargement de région (pas de fuite).
