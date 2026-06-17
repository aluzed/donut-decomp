# Tickets Physics

## PHYS-001 — Réactiver stepSimulation
**Priorité :** P0 (bloquant)
**Fichier :** `src/Physics/WorldPhysics.cpp` (Update)
**Description :** `mDynamicsWorld->stepSimulation(dt)` est commenté. Le monde physique existe mais ne simule rien. Les personnages, véhicules et objets traversent le décor.
**Correctif :** Décommenter `stepSimulation`. Vérifier que le dt est correct (fixe ou variable).

---

## PHYS-002 — Finaliser CharacterController
**Priorité :** P0 (bloquant)
**Fichier :** `src/CharacterController.cpp`
**Tâches :**
- [ ] Implémenter `canJump()` (détection du sol via raycast)
- [ ] Implémenter `jump()` (vélocité verticale)
- [ ] Implémenter `onGround()` 
- [ ] Implémenter `debugDraw()` (rendu debug du controller)
- [ ] Implémenter `reset()` (remise à zéro)
- [ ] Implémenter `setVelocityForTimeInterval()` (impulsion)
- [ ] Réparer le destructeur (cleanup Bullet commenté)
- [ ] Reverser les bonnes valeurs de gravité du jeu original (probablement ~9.8 m/s², pas 1.0)
- [ ] Fix le `convexSweepTest` commenté "this doesn't work TWAT"

---

## PHYS-003 — Intégrer les collisions statiques dans le monde
**Priorité :** P1
**Fichier :** `src/Level.cpp` (LoadP3D)
**Description :** Les appels à `AddCollisionVolume`, `AddP3DOBBoxVolume`, `AddP3DFence`, `AddIntersect` sont commentés. Le monde physique n'a pas de géométrie de collision.
**Correctif :** Décommenter et valider que chaque type de volume est correctement converti vers Bullet.

---

## PHYS-004 — Réparer les rotations des collisions
**Priorité :** P1
**Fichier :** `src/Physics/WorldPhysics.cpp`
**Description :** Plusieurs TODO :
- `// todo: apply rotation` pour les Fence
- `// todo: not right` pour les Cylinder (rotation)
- `// todo: wrong` pour les transformations
**Correctif :** Reverser les matrices de transformation P3D → Bullet correctement.

---

## PHYS-005 — Utiliser BulletFenceShape au lieu de btBoxShape
**Priorité :** P2
**Fichiers :** `src/Physics/WorldPhysics.cpp` (AddP3DFence), `src/Physics/BulletFenceShape.cpp`
**Description :** `AddP3DFence` crée un `btBoxShape` fin au lieu d'utiliser le `BulletFenceShape` défini dans le projet. La fence shape est mieux adaptée aux longs murs fins.
**Correctif :** Utiliser `BulletFenceShape` dans `AddP3DFence`.

---

## PHYS-006 — Implémenter le CharacterController via Bullet standard
**Priorité :** P3 (alternative)
**Description :** Évaluer si `btKinematicCharacterController` de Bullet peut remplacer le custom `CharacterController` actuel. Le custom est buggé et incomplet ; celui de Bullet est stable.

---

## PHYS-007 — Physique des véhicules (raycast vehicle)
**Priorité :** P1
**Description :** Implémenter un `btRaycastVehicle` ou `btRaycastVehicle2` pour les voitures. Le jeu original utilise un véhicule arcade avec :
- Suspensions par rayon
- Moteur avec boîte de vitesse
- Freinage, drift, nitro
- Le fichier `dev/Commands.md` liste ~30 paramètres de tuning véhicule (SetMass, SetGasScale, SetTopSpeedKmh, SetTireGrip, SetSuspensionLimit...)

---

## PHYS-008 — Types de surface (SurfaceTypeList)
**Priorité :** P2
**Description :** Le jeu a des propriétés physiques différentes par surface (route, herbe, trottoir, sable...). Implémenter via les `SurfaceTypeList` chunks une fois reversés (P3D-008).

---

## PHYS-009 — Collision avec les fences (murs invisibles)
**Priorité :** P1
**Description :** Les fences P3D sont des murs de collision invisibles autour des niveaux. Activer leur chargement et les ajouter au monde Bullet.

---

## PHYS-010 — Collision avec les Intersect meshes
**Priorité :** P1
**Description :** Les Intersect sont des meshes de collision optimisés (low-poly). Les activer dans `Level::LoadP3D` (AddIntersect commenté).
