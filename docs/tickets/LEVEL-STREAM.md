# LEVEL-STREAM — Streaming par distance (load/unload de zones selon la position joueur)

- **Status:** TODO
- **Priority:** P1
- **Module:** Level
- **Depends on:** —
- **Files:** `src/Level.cpp:227-251` (`DynaLoadData`), `src/Level.cpp:302-318` (`loadRegion`/`unloadRegion`), `src/Level.cpp:319+` (`Update`), `src/Level.h`

## Contexte
`DynaLoadData` (`Level.cpp:227-251`) parse déjà les listes de régions à charger/décharger (séparateurs `;:@$`) et appelle `loadRegion`/`unloadRegion` (`Level.cpp:302-318`). Mais ce déclenchement n'est pas piloté par la position du joueur : il n'existe ni streaming par distance ni culling de viewport. Tout reste chargé.

## Approche
1. Associer chaque région à une emprise spatiale (AABB/centre+rayon) — déductible des locators/bounds déjà parsés (`Level.cpp:177-199`) ou d'un mapping fourni par `DynaLoadData`.
2. Dans `Level::Update` (`Level.cpp:319+`), comparer la position joueur aux emprises : charger les régions dans un rayon `loadRadius`, décharger au-delà d'un `unloadRadius > loadRadius` (hystérésis anti-thrash), en réutilisant `loadRegion`/`unloadRegion` et le garde `_loadedRegions` existant.
3. Étaler les chargements (au plus une région par frame, ou en tâche) pour éviter les pics.
4. (Optionnel) `unloadRegion` doit aussi libérer les bodies physiques associés (cf. PHYS-011).

## Critères d'acceptation
- [ ] Chaque région a une emprise spatiale exploitable.
- [ ] Les régions se chargent/déchargent selon la distance au joueur dans `Level::Update`.
- [ ] Hystérésis (`loadRadius` < `unloadRadius`) : pas de cycle load/unload répété en bordure.
- [ ] `_loadedRegions` reste cohérent (pas de double chargement).
- [ ] Le set de régions chargées diminue quand le joueur s'éloigne.
