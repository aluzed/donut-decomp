# VEH-DAMAGE — Dégâts visuels du véhicule (bascule vers car_damage.p3d à l'impact)

- **Status:** TODO
- **Priority:** P3
- **Module:** Vehicle
- **Depends on:** —
- **Files:** `src/Vehicle.cpp:22-30` (`LoadModel`), `src/Vehicle.cpp:90-104` (`Update`), `src/Vehicle.cpp:163-167` (`Draw`), `src/Vehicle.h` (membres `_model`/`_name`), `src/Game.cpp:571-576` (« collision damage disabled »)

## Contexte
`Vehicle` charge un seul modèle via `LoadModel` (`Vehicle.cpp:22-30`, stocké dans `_model`) et
le dessine tel quel (`Draw`, `Vehicle.cpp:163-167`). Aucun état de dégât visuel n'existe : à
l'impact, le mesh reste intact. Le bloc de dégâts côté Game est par ailleurs désactivé
(`Game.cpp:571-576`).

## Approche
1. Charger un second modèle endommagé dans `Vehicle` : `LoadModel` (`Vehicle.cpp:22-30`)
   conserve `_model` (intact) et charge en plus `_damagedModel` depuis `car_damage.p3d` (via
   `CompositeModel::LoadP3D`, comme `Vehicle.cpp:24`).
2. Ajouter un état `_damaged`/seuil de dégâts (champ float) à `Vehicle.h` et une méthode
   `OnImpact(float force)` qui l'incrémente et bascule `_damaged = true` au-delà d'un seuil.
3. Dans `Draw` (`Vehicle.cpp:163-167`), dessiner `_damagedModel` quand `_damaged` est vrai,
   sinon `_model` ; appliquer le même `GetTransform()`.
4. Appeler `OnImpact` depuis la détection de collision véhicule côté Game (réactiver/brancher
   le bloc `Game.cpp:571-576`), ou via un contact Bullet sur le chassis.

## Critères d'acceptation
- [ ] `Vehicle` charge `car_damage.p3d` en plus du modèle intact.
- [ ] Un impact suffisant bascule le rendu sur le modèle endommagé.
- [ ] `Draw` choisit le bon modèle selon l'état de dégât, même transform.
- [ ] La bascule est déclenchée par une collision réelle (pas seulement par debug).
- [ ] Le jeu compile et reste runnable.
