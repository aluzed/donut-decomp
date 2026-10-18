# GAME-014 — Intérieurs (maison Simpson, école, Kwik-E-Mart) chargés dynamiquement via Interior/Locator

- **Status:** PARTIAL (2026-10-18)
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

## Done (2026-10-18)

Level 1's interiors are their own P3D files, `l1i00` (Springfield Elementary, `int_ele_*`),
`l1i01` (Kwik-E-Mart, `int_kwik_*`) and `l1i02` (the Simpsons' house, `int_sh_*`). Each is
laid out **under the map** at x ~ 500, y = -20, one beside the next. They are loaded at
start-up. (The CD extraction had them in `files/extracted/Art` but not in `art/`; ASSETS.md's
`cp -a files/extracted/Art/. art/` brings them over.)

- **Per-file layout.** Every interior has an `InteriorEntryStart`, so the level-wide locator
  table keeps only one of them. `Level` reads each file's own: where the player comes in,
  the `*_exit` spot that takes them out, and `InteriorOrigin`.
- **Going in.** A door is a type 7 locator whose trigger carries the interior's name
  (LEVEL-TRIG). Walking into it, on foot only, puts the player at the interior's entry, facing
  its middle. Which file holds which name is not in the data; a three-line table follows the
  files' locator prefixes.
- **Inside**, only that file's geometry is drawn (`Level::SetDrawOnly`). Otherwise the rest
  of the level is the town overhead, seen from underneath.
- **Coming out.** The exit works once the player has stepped away from it (they arrive next
  to it). It puts them 4m in front of the door, clear of its trigger.
- Missions: `interior` is met by actually going in (the door's marker no longer completes it
  from the car), `gooutside` by coming out. In m1 the player walks into the school and talks to
  Lisa in there.

Not done: interiors are not streamed (`DynaLoadData`), and the follow camera still clips
through interior walls.

## Critères d'acceptation
- [ ] Entrer dans la maison Simpson, l'école et le Kwik-E-Mart charge dynamiquement l'intérieur via `DynaLoadData`.
- [x] La sortie restaure la zone extérieure (the player is put back in front of the door, the whole level drawn again).
- [ ] Le mapping trigger→zones réutilise/étend la table `locations`.
- [ ] Plus de chargement statique en dur de toutes les rooms au boot (streaming à la place).
- [ ] Le jeu compile et reste jouable.
