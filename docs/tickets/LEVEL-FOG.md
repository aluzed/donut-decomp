# LEVEL-FOG — Brouillard par zone depuis les paramètres P3D

- **Status:** TODO
- **Priority:** P2
- **Module:** Level
- **Depends on:** —
- **Files:** `src/Level.cpp:329-330` (uniformes fog en dur), `src/Level.cpp:176` (WorldSphere), `src/Level.cpp:319+` (`Update`), `src/Level.h`

## Contexte
Le brouillard est aujourd'hui codé en dur : `_worldShader->SetUniformValue("fogDensity", 0.0004f)` et `fogColor(0.62, 0.78, 1.0, 1.0)` (`Level.cpp:329-330`). Les valeurs ne proviennent pas du P3D et ne varient pas par zone. Le P3D porte des paramètres de brouillard (densité, couleur, distances) associés aux zones/world.

## Approche
1. Parser les paramètres de brouillard du P3D (au chargement du `WorldSphere`/zone, `Level.cpp:176`) et les stocker par zone (`fogDensity`, `fogColor`, distances near/far).
2. Sélectionner la zone courante selon la position joueur (réutiliser l'emprise spatiale de LEVEL-STREAM si dispo) dans `Level::Update`.
3. Remplacer les valeurs en dur (`Level.cpp:329-330`) par les paramètres de la zone active, avec interpolation douce lors des transitions pour éviter les sauts.
4. Conserver un fallback (valeurs actuelles) si la zone n'a pas de paramètres de brouillard.

## Critères d'acceptation
- [ ] Les paramètres de brouillard sont lus depuis le P3D, plus de valeurs en dur.
- [ ] Le brouillard change selon la zone où se trouve le joueur.
- [ ] Transition douce (interpolation) entre deux zones.
- [ ] Fallback propre si une zone n'a pas de paramètres de brouillard.
