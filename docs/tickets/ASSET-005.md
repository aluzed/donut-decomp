# ASSET-005 — Charger un vrai modèle de personnage (P3D) à la place de SimpleMesh

- **Status:** TODO
- **Priority:** P1
- **Module:** Tools
- **Depends on:** ASSET-004
- **Files:** `src/Game.h:112-113` (`_playerMesh`/`_carMesh` en `SimpleMesh`), `src/Game.cpp:144` (`TextureFont::Load`), `src/Game.h:120-122` (`_animP3D`/`_globalP3D`)

## Contexte
Le joueur est rendu via un `SimpleMesh` placeholder (`src/Game.h:112`). Il faut charger un vrai personnage P3D extrait (`art/chars/homer_m.p3d`) et vérifier le skinning et l'animation, le pipeline anim existant utilisant déjà `_animP3D` (`src/Game.h:120`).

## Approche
- Charger `art/chars/homer_m.p3d` via `P3D::P3DFile` (cf. `src/FrontendProject.cpp:115`) et extraire le mesh skinné + squelette + textures.
- Remplacer `_playerMesh` (`src/Game.h:112`) par le modèle P3D rendu via le `_skinShaderProgram` existant (`src/Game.h:124`).
- Brancher les animations (idle/walk) déjà présentes côté `Character` sur le squelette du P3D ; vérifier la cohérence du bind pose (cf. correctifs récents bind pose).
- Vérifier visuellement skinning correct (pas d'aplatissement, cf. commit `homer isn't flattened`) et la lecture des clips depuis le P3D d'anim.

## Critères d'acceptation
- [ ] `homer_m.p3d` chargé et affiché à la place du `SimpleMesh`.
- [ ] Skinning correct (pose et déformations valides).
- [ ] Animations idle/walk jouées sur le squelette du modèle.
