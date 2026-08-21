# CHARCTRL-FALL — Le personnage traverse le sol dès qu'il se déplace

- **Status:** TODO
- **Priority:** P0 (bloquant : le jeu n'est pas jouable à pied)
- **Module:** Physics / CharacterController
- **Depends on:** —
- **Blocks:** toute vérification visuelle du gameplay à pied (UI-HUD, GAME-010, LEVEL-TRIG)
- **Files:** `src/CharacterController.cpp` (`stepDown`, `stepForwardAndStrafe`, `onGround`), `src/Game.cpp:735` (garde de respawn)

## Contexte

Découvert en validant `REGR-001` (2026-08-22, Windows / GCC 16 / UCRT).

À l'arrêt, le personnage tient correctement sur le terrain (Y stable à ~3.4). **Dès qu'il
avance**, il passe au travers du sol et tombe. Le garde `Game.cpp` (« player fell off the
map, respawning ») le remet à `_lastSafePos`, il retombe aussitôt, et la boucle se répète :

- 25 s de marche avant → **21 respawns** ;
- 90 s de marche + conduite → **100 respawns**.

Comme `_lastSafePos` n'est mis à jour que quand `onGround()` est vrai et qu'il ne l'est
jamais en chute, le joueur est renvoyé indéfiniment au même point.

## Ce qui est déjà écarté

Test A/B avec le bloc gameplay de `REGR-001` **désactivé** : la chute se produit à
l'identique (21 fois sur 25 s). Ce n'est donc **pas** dû aux piétons / collectibles /
triggers, mais bien au contrôleur de personnage ou à la collision du terrain.

## Pistes d'investigation

- `9860a4f` a retouché `stepUp`/`stepDown`/`onGround` (offset de montée rendu par
  `stepDown`, balayage depuis `_targetPosition`, raycast `onGround` allongé). Un de ces
  correctifs peut avoir déplacé le problème plutôt que de le résoudre.
- Vérifier que le balayage vers le bas de `stepDown()` teste bien contre le mesh de
  collision du terrain, et pas seulement contre les volumes P3D : le sol de `L1_TERRA.p3d`
  est-il ajouté au monde Bullet comme `btBvhTriangleMeshShape` ?
- Un `btBvhTriangleMeshShape` statique laisse passer les objets rapides : activer le CCD
  (`setCcdMotionThreshold` / `setCcdSweptSphereRadius`) sur la capsule, ou vérifier que le
  déplacement par frame ne dépasse pas l'épaisseur du triangle.
- Instrumenter `onGround()` pendant le déplacement : renvoie-t-il `false` dès la première
  frame de mouvement, ou seulement après que la chute a commencé ?

## Critères d'acceptation

- [ ] Marcher 60 s dans une direction quelconque n'émet **aucun** « fell off the map ».
- [ ] `onGround()` reste vrai en marche sur terrain plat.
- [ ] Le personnage monte les trottoirs sans gagner ni perdre d'altitude parasite.
- [ ] La cause racine est identifiée (pas un simple relèvement du seuil de respawn).
