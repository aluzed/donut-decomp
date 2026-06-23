# PHYS-008 — Friction par surface (route / herbe / trottoir / sable)

- **Status:** TODO
- **Priority:** P2
- **Module:** Physics
- **Depends on:** —
- **Files:** `src/Vehicle.h:42`, `src/Vehicle.h:61`, `src/Vehicle.cpp`, `src/Physics/WorldPhysics.cpp`

## Contexte
Le grip est aujourd'hui une constante par véhicule (`_tireGrip = 1.0f`, `Vehicle.h:61`, réglée via `SetTireGrip`, `Vehicle.h:42`) : aucune notion de matériau au sol. Le P3D fournit `SurfaceTypeList` (related P3D-008) associant des triangles à un type de surface (route, herbe, trottoir, sable).

## Approche
1. Parser `SurfaceTypeList` (P3D-008) et stocker une table triangle/zone → `SurfaceType` accessible depuis `WorldPhysics`.
2. Dans la boucle physique du véhicule (`Vehicle.cpp`), récupérer le matériau sous chaque roue via le raycast existant du `btRaycastVehicle` (le `btCollisionObject`/triangle touché donne le `SurfaceType`).
3. Définir un coefficient de friction par `SurfaceType` (ex. route 1.0, trottoir 0.9, herbe 0.6, sable 0.4) et moduler `_tireGrip` effectif (`m_frictionSlip` des `btWheelInfo`) au lieu d'une valeur fixe.
4. Garder `SetTireGrip` comme multiplicateur de base par véhicule.

## Critères d'acceptation
- [ ] Une table `SurfaceType` → coefficient de friction existe et est alimentée depuis le P3D.
- [ ] La friction effective de chaque roue dépend de la surface sous la roue.
- [ ] Conduite hors route (herbe/sable) sensiblement plus glissante que sur route.
- [ ] `SetTireGrip` reste fonctionnel comme facteur par véhicule.
