# GAME-014 — Intérieurs (maison Simpson, école, Kwik-E-Mart) chargés dynamiquement via Interior/Locator

- **Status:** TODO
- **Priority:** P1
- **Module:** Game
- **Depends on:** —
- **Files:** `src/Game.cpp:153-167` (chargement P3D intérieurs + `DynaLoadData`), `src/Game.cpp:344-356` (`locations` avec listes dyna), `src/Level.h` (`DynaLoadData`/`CheckTrigger`), `src/Game.cpp:695-708` (`CheckTrigger` désactivé)

## Contexte
Les intérieurs sont déjà chargés statiquement au boot : maison Simpson (`l1z1/l1r1/l1r7`,
`Game.cpp:154-156`), puis `DynaLoadData("l1z1.p3d;l1r1.p3d;l1r7.p3d;")` `Game.cpp:167`. La table
`locations` (`Game.cpp:344-356`) associe chaque lieu à une liste de zones dyna (ex. Kwik-E-Mart
`l1z2;l1r1;l1r2`, École `l1z3;l1r2;l1r3`). Mais le chargement n'est jamais déclenché par
l'entrée du joueur : le `CheckTrigger` qui le ferait est dans le bloc commenté (`Game.cpp:695-708`).

## Approche
1. Introduire un concept `Interior`/`Locator` : map d'un déclencheur de porte (position +
   rayon, ou locator P3D) vers une chaîne `DynaLoadData` (réutiliser le 3e champ de `locations`,
   `Game.cpp:344-356`).
2. À l'entrée d'un trigger d'intérieur (via `Level::CheckTrigger`, à réactiver — cf. bloc
   `Game.cpp:695-708`), appeler `_level->DynaLoadData(zones)` (`Game.cpp:167`) pour streamer la
   bonne zone, et téléporter le joueur au point d'entrée intérieur.
3. À la sortie, recharger la zone extérieure de retour.
4. Retirer le chargement statique en dur des rooms (`Game.cpp:154-166`) une fois le streaming
   dynamique fonctionnel (garder L1_TERRA et les zones de base).

## Critères d'acceptation
- [ ] Entrer dans la maison Simpson, l'école et le Kwik-E-Mart charge dynamiquement l'intérieur via `DynaLoadData`.
- [ ] La sortie restaure la zone extérieure.
- [ ] Le mapping trigger→zones réutilise/étend la table `locations`.
- [ ] Plus de chargement statique en dur de toutes les rooms au boot (streaming à la place).
- [ ] Le jeu compile et reste jouable.
